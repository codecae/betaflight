/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>
 *
 * + Updated 02-SEP-17 - decoupled SmartPort into msp_shared.h - null
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef TELEMETRY

#include "config/feature.h"
#include "build/build_config.h"
#include "build/version.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "common/crc.h"
#include "common/maths.h"
#include "common/printf.h"
#include "common/streambuf.h"
#include "common/utils.h"

#include "sensors/battery.h"

#include "io/gps.h"
#include "io/serial.h"

#include "fc/rc_modes.h"
#include "fc/runtime_config.h"

#include "io/gps.h"

#include "flight/imu.h"

#include "rx/rx.h"
#include "rx/crsf.h"

#include "telemetry/telemetry.h"
#include "telemetry/crsf.h"
#include "telemetry/msp_shared.h"

#include "fc/config.h"

#define CRSF_CYCLETIME_US                   100000 // 100ms, 10 Hz
#define CRSF_DEVICEINFO_VERSION             0x01
#define CRSF_DEVICEINFO_PARAMETER_COUNT     255

static bool crsfTelemetryEnabled;
static bool deviceInfoReplyPending;
static bool mspReplyPending;
static uint8_t crsfFrame[CRSF_FRAME_SIZE_MAX];

static void crsfInitializeFrame(sbuf_t *dst, uint8_t originAddr)
{
    dst->ptr = crsfFrame;
    dst->end = ARRAYEND(crsfFrame);

    sbufWriteU8(dst, originAddr);
}

static void crsfWriteCrc(sbuf_t *dst, uint8_t *start)
{
    uint8_t crc = 0;
    uint8_t *end = sbufPtr(dst);
    for (uint8_t *ptr = start; ptr < end; ++ptr) {
        crc = crc8_dvb_s2(crc, *ptr);
    }
    sbufWriteU8(dst, crc);
}

static void crsfFinalize(sbuf_t *dst)
{
    crsfWriteCrc(dst, &crsfFrame[2]); // start at byte 2, since CRC does not include device address and frame length
    sbufSwitchToReader(dst, crsfFrame);
    // write the telemetry frame to the receiver.
    crsfRxWriteTelemetryData(sbufPtr(dst), sbufBytesRemaining(dst));
}

static int crsfFinalizeBuf(sbuf_t *dst, uint8_t *frame)
{
    crsfWriteCrc(dst, &crsfFrame[2]); // start at byte 2, since CRC does not include device address and frame length
    sbufSwitchToReader(dst, crsfFrame);
    const int frameSize = sbufBytesRemaining(dst);
    for (int ii = 0; sbufBytesRemaining(dst); ++ii) {
        frame[ii] = sbufReadU8(dst);
    }
    return frameSize;
}

/*
CRSF frame has the structure:
<Device address> <Frame length> <Type> <Payload> <CRC>
Device address: (uint8_t)
Frame length:   length in  bytes including Type (uint8_t)
Type:           (uint8_t)
CRC:            (uint8_t), crc of <Type> and <Payload>
*/

/*
0x02 GPS
Payload:
int32_t     Latitude ( degree / 10`000`000 )
int32_t     Longitude (degree / 10`000`000 )
uint16_t    Groundspeed ( km/h / 10 )
uint16_t    GPS heading ( degree / 100 )
uint16      Altitude ( meter ­1000m offset )
uint8_t     Satellites in use ( counter )
*/
void crsfFrameGps(sbuf_t *dst)
{
    // use sbufWrite since CRC does not include frame length
    sbufWriteU8(dst, CRSF_FRAME_GPS_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_GPS);
    sbufWriteU32BigEndian(dst, gpsSol.llh.lat); // CRSF and betaflight use same units for degrees
    sbufWriteU32BigEndian(dst, gpsSol.llh.lon);
    sbufWriteU16BigEndian(dst, (gpsSol.groundSpeed * 36 + 5) / 10); // gpsSol.groundSpeed is in 0.1m/s
    sbufWriteU16BigEndian(dst, gpsSol.groundCourse * 10); // gpsSol.groundCourse is degrees * 10
    //Send real GPS altitude only if it's reliable (there's a GPS fix)
    const uint16_t altitude = (STATE(GPS_FIX) ? gpsSol.llh.alt : 0) + 1000;
    sbufWriteU16BigEndian(dst, altitude);
    sbufWriteU8(dst, gpsSol.numSat);
}

/*
0x08 Battery sensor
Payload:
uint16_t    Voltage ( mV * 100 )
uint16_t    Current ( mA * 100 )
uint24_t    Capacity ( mAh )
uint8_t     Battery remaining ( percent )
*/
void crsfFrameBatterySensor(sbuf_t *dst)
{
    // use sbufWrite since CRC does not include frame length
    sbufWriteU8(dst, CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_BATTERY_SENSOR);
    sbufWriteU16BigEndian(dst, getBatteryVoltage()); // vbat is in units of 0.1V
    sbufWriteU16BigEndian(dst, getAmperage() / 10);
    const uint32_t batteryCapacity = batteryConfig()->batteryCapacity;
    const uint8_t batteryRemainingPercentage = calculateBatteryPercentageRemaining();
    sbufWriteU8(dst, (batteryCapacity >> 16));
    sbufWriteU8(dst, (batteryCapacity >> 8));
    sbufWriteU8(dst, (uint8_t)batteryCapacity);
    sbufWriteU8(dst, batteryRemainingPercentage);
}

typedef enum {
    CRSF_ACTIVE_ANTENNA1 = 0,
    CRSF_ACTIVE_ANTENNA2 = 1
} crsfActiveAntenna_e;

typedef enum {
    CRSF_RF_MODE_4_HZ = 0,
    CRSF_RF_MODE_50_HZ = 1,
    CRSF_RF_MODE_150_HZ = 2
} crsrRfMode_e;

typedef enum {
    CRSF_RF_POWER_0_mW = 0,
    CRSF_RF_POWER_10_mW = 1,
    CRSF_RF_POWER_25_mW = 2,
    CRSF_RF_POWER_100_mW = 3,
    CRSF_RF_POWER_500_mW = 4,
    CRSF_RF_POWER_1000_mW = 5,
    CRSF_RF_POWER_2000_mW = 6
} crsrRfPower_e;

/*
0x1E Attitude
Payload:
int16_t     Pitch angle ( rad / 10000 )
int16_t     Roll angle ( rad / 10000 )
int16_t     Yaw angle ( rad / 10000 )
*/

#define DECIDEGREES_TO_RADIANS10000(angle) ((int16_t)(1000.0f * (angle) * RAD))

void crsfFrameAttitude(sbuf_t *dst)
{
     sbufWriteU8(dst, CRSF_FRAME_ATTITUDE_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
     sbufWriteU8(dst, CRSF_FRAMETYPE_ATTITUDE);
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.pitch));
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.roll));
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.yaw));
}

/*
0x21 Flight mode text based
Payload:
char[]      Flight mode ( Null terminated string )
*/
void crsfFrameFlightMode(sbuf_t *dst)
{
    // write zero for frame length, since we don't know it yet
    uint8_t *lengthPtr = sbufPtr(dst);
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, CRSF_FRAMETYPE_FLIGHT_MODE);

    // use same logic as OSD, so telemetry displays same flight text as OSD
    const char *flightMode = "ACRO";
    if (isAirmodeActive()) {
        flightMode = "AIR";
    }
    if (FLIGHT_MODE(FAILSAFE_MODE)) {
        flightMode = "!FS";
    } else if (FLIGHT_MODE(ANGLE_MODE)) {
        flightMode = "STAB";
    } else if (FLIGHT_MODE(HORIZON_MODE)) {
        flightMode = "HOR";
    }
    sbufWriteString(dst, flightMode);
    sbufWriteU8(dst, '\0');     // zero-terminate string
    // write in the frame length
    *lengthPtr = sbufPtr(dst) - lengthPtr;
}

void scheduleDeviceInfoResponse() {
    deviceInfoReplyPending = true;
}

/*
0x29 Device Info
Payload:
uint8_t     Destination
uint8_t     Origin
char[]      Device Name ( Null terminated string )
uint32_t    Null Bytes
uint32_t    Null Bytes
uint32_t    Null Bytes
uint8_t     255 (Max MSP Parameter)
uint8_t     0x01 (Parameter version 1)
*/
void crsfFrameDeviceInfo(sbuf_t *dst) {

    char buff[30];
    tfp_sprintf(buff, "%s %s: %s", FC_FIRMWARE_NAME, FC_VERSION_STRING, systemConfig()->boardIdentifier);

    uint8_t *lengthPtr = sbufPtr(dst);
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, CRSF_FRAMETYPE_DEVICE_INFO);
    sbufWriteU8(dst, CRSF_ADDRESS_RADIO_TRANSMITTER);
    sbufWriteU8(dst, CRSF_ADDRESS_BETAFLIGHT);
    sbufWriteString(dst, buff);
    sbufWriteU8(dst, '\0');
    for (unsigned int ii=0; ii<12; ii++) {
        sbufWriteU8(dst, 0x00);
    }
    sbufWriteU8(dst, CRSF_DEVICEINFO_PARAMETER_COUNT);
    sbufWriteU8(dst, CRSF_DEVICEINFO_VERSION);
    *lengthPtr = sbufPtr(dst) - lengthPtr;
}

#define BV(x)  (1 << (x)) // bit value

// schedule array to decide how often each type of frame is sent
#define CRSF_SCHEDULE_COUNT_MAX     6
static uint8_t crsfScheduleCount;
static uint8_t crsfSchedule[CRSF_SCHEDULE_COUNT_MAX];

/*
0x7A - MSP Request - Incoming
-----------------------------
uint8_t Device Address
uint8_t Length (including type and CRC)
uint8_t Frame Type
uint8_t Destination Address
uint8_t Origin Address
<8 bytes of msp data - zero padded>
uint8_t CRC

0x7B - MSP Response - Outgoing
------------------------------
uint8_t Device Address
uint8_t Length (including Type and CRC)
uint8_t Frame Type
uint8_t Destination Address
uint8_t Origin Address
<58 bytes of msp data - zero padded>
uint8_t CRC

0x7C - MSP Write - Incoming
----------------------------
uint8_t Device Address
uint8_t Length (including type and CRC)
uint8_t Frame Type
uint8_t Destination Address
uint8_t Origin Address
<8 bytes of msp data - chunked>
uint8_t CRC

Incoming write packets are limited in size by OpenTx. 
MSP write data can be sequenced in chunks to enable larger transports.

*/

void scheduleMspResponse() {
    if (!mspReplyPending) {
        mspReplyPending = true;
    }
}

void crsfSendMspResponse(uint8_t *packet) 
{
    sbuf_t crsfPayloadBuf;
    sbuf_t mspPayload;
    sbuf_t *dst = &crsfPayloadBuf;
    sbuf_t *msp = &mspPayload;

    msp->ptr = packet;
    msp->end = packet + CRSF_FRAME_TX_MSP_PAYLOAD_SIZE;

    crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
    sbufWriteU8(dst, CRSF_FRAME_TX_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_MSP_RESP);
    sbufWriteU8(dst, CRSF_ADDRESS_RADIO_TRANSMITTER);
    sbufWriteU8(dst, CRSF_ADDRESS_BETAFLIGHT);
    while (sbufBytesRemaining(msp)) {
        sbufWriteU8(dst, sbufReadU8(msp));
    }
    crsfFinalize(dst);
 }

static void processCrsf(void)
{
    static uint8_t crsfScheduleIndex = 0;
    const uint8_t currentSchedule = crsfSchedule[crsfScheduleIndex];

    sbuf_t crsfPayloadBuf;
    sbuf_t *dst = &crsfPayloadBuf;

    if (currentSchedule & BV(CRSF_FRAME_ATTITUDE)) {
        crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
        crsfFrameAttitude(dst);
        crsfFinalize(dst);
    }
    if (currentSchedule & BV(CRSF_FRAME_BATTERY_SENSOR)) {
        crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
        crsfFrameBatterySensor(dst);
        crsfFinalize(dst);
    }
    if (currentSchedule & BV(CRSF_FRAME_FLIGHT_MODE)) {
        crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
        crsfFrameFlightMode(dst);
        crsfFinalize(dst);
    }
#ifdef GPS
    if (currentSchedule & BV(CRSF_FRAME_GPS)) {
        crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
        crsfFrameGps(dst);
        crsfFinalize(dst);
    }
#endif
    if ((currentSchedule & BV(CRSF_FRAME_DEVICE_INFO)) && deviceInfoReplyPending) {
        crsfInitializeFrame(dst, CRSF_ADDRESS_BROADCAST);
        crsfFrameDeviceInfo(dst);
        crsfFinalize(dst);
        deviceInfoReplyPending = false;
    }
    /*if (currentSchedule & BV(CRSF_FRAME_MSP_RESPONSE) && mspReplyPending) {
       mspReplyPending = sendMspReply(CRSF_FRAME_TX_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
       if (mspReplyPending) {
            crsfScheduleIndex--;    
       }
    } */
    crsfScheduleIndex = (crsfScheduleIndex + 1) % crsfScheduleCount;
}

void initCrsfTelemetry(void)
{
    // check if there is a serial port open for CRSF telemetry (ie opened by the CRSF RX)
    // and feature is enabled, if so, set CRSF telemetry enabled
    crsfTelemetryEnabled = crsfRxIsActive();

    deviceInfoReplyPending = false;
    mspReplyPending = false;
    
    int index = 0;
    crsfSchedule[index++] = BV(CRSF_FRAME_ATTITUDE);
    crsfSchedule[index++] = BV(CRSF_FRAME_BATTERY_SENSOR);
    crsfSchedule[index++] = BV(CRSF_FRAME_FLIGHT_MODE);
    if (feature(FEATURE_GPS)) {
        crsfSchedule[index++] = BV(CRSF_FRAME_GPS);
    }
    crsfSchedule[index++] = BV(CRSF_FRAME_DEVICE_INFO);
    //crsfSchedule[index++] = BV(CRSF_FRAME_MSP_RESPONSE);
    crsfScheduleCount = (uint8_t)index;

 }

bool checkCrsfTelemetryState(void)
{
    return crsfTelemetryEnabled;
}

/*
 * Called periodically by the scheduler
 */
void handleCrsfTelemetry(timeUs_t currentTimeUs)
{
    static uint32_t crsfLastCycleTime;

    if (!crsfTelemetryEnabled) {
        return;
    }
    // Give the receiver a chance to send any outstanding telemetry data.
    // This needs to be done at high frequency, to enable the RX to send the telemetry frame
    // in between the RX frames.
    crsfRxSendTelemetryData();

    // Actual telemetry data only needs to be sent at a low frequency, ie 10Hz
    if (currentTimeUs >= crsfLastCycleTime + CRSF_CYCLETIME_US) {
        crsfLastCycleTime = currentTimeUs;
        if (mspReplyPending) {
            mspReplyPending = sendMspReply(CRSF_FRAME_TX_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
        } else {
            processCrsf();
        }
    }
}

int getCrsfFrame(uint8_t *frame, crsfFrameType_e frameType)
{
    sbuf_t crsfFrameBuf;
    sbuf_t *sbuf = &crsfFrameBuf;

    crsfInitializeFrame(sbuf, CRSF_ADDRESS_BROADCAST);
    switch (frameType) {
    default:
    case CRSF_FRAME_ATTITUDE:
        crsfFrameAttitude(sbuf);
        break;
    case CRSF_FRAME_BATTERY_SENSOR:
        crsfFrameBatterySensor(sbuf);
        break;
    case CRSF_FRAME_FLIGHT_MODE:
        crsfFrameFlightMode(sbuf);
        break;
#if defined(GPS)
    case CRSF_FRAME_GPS:
        crsfFrameGps(sbuf);
        break;
#endif
    }
    const int frameSize = crsfFinalizeBuf(sbuf, frame);
    return frameSize;
}
#endif
