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
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <limits.h>
#include <algorithm>

extern "C" {
    #include <platform.h>

    #include "build/debug.h"

    #include "common/crc.h"
    #include "common/utils.h"
    #include "common/printf.h" 
    #include "common/gps_conversion.h"
    #include "common/typeconversion.h"

    #include "config/parameter_group.h"
    #include "config/parameter_group_ids.h"

    #include "drivers/serial.h"
    #include "drivers/system.h"

    #include "fc/runtime_config.h"
    #include "fc/config.h"
    #include "flight/imu.h"
    #include "fc/fc_msp.h"

    #include "io/serial.h"
    #include "io/gps.h"

    #include "msp/msp.h"

    #include "rx/rx.h"
    #include "rx/crsf.h"

    #include "sensors/battery.h"
    #include "sensors/sensors.h"

    #include "telemetry/telemetry.h"
    #include "telemetry/msp_shared.h"

    void crsfDataReceive(uint16_t c);
    uint8_t crsfFrameCRC(void);
    uint16_t crsfReadRawRC(const rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan);
    uint16_t testBatteryVoltage = 0;
    int32_t testAmperage = 0;

    PG_REGISTER(batteryConfig_t, batteryConfig, PG_BATTERY_CONFIG, 0);
    PG_REGISTER(telemetryConfig_t, telemetryConfig, PG_TELEMETRY_CONFIG, 0);
    PG_REGISTER(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG, 0);

    extern bool crsfFrameDone;
    extern crsfFrame_t crsfFrame;

    uint32_t dummyTimeUs;

}

#include "unittest_macros.h"
#include "gtest/gtest.h"

typedef struct crsfMspFrame_s {
    uint8_t deviceAddress;
    uint8_t frameLength;
    uint8_t type;
    uint8_t destination;
    uint8_t origin;
    uint8_t payload[CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_CRC];
} crsfMspFrame_t;

const uint8_t crsfPidRequest[] = {
    0x00,0x0D,0x7A,0xC8,0xEA,0x30,0x00,0x70,0x70,0x00,0x00,0x00,0x00,0x69
};

TEST(CrossFireMSPTest, TestCrsfPIDRequest)
{
    
    const crsfMspFrame_t *framePtr = (const crsfMspFrame_t*)crsfPidRequest;
    crsfFrame = *(const crsfFrame_t*)framePtr;
    crsfFrameDone = true;
    const uint8_t crc = crsfFrameCRC();
    EXPECT_EQ(true, crsfFrameDone);
    EXPECT_EQ(crc, crsfFrame.frame.payload[2+CRSF_FRAME_MSP_PAYLOAD_SIZE]);
    EXPECT_EQ(CRSF_ADDRESS_BROADCAST, crsfFrame.frame.deviceAddress);
    EXPECT_EQ(CRSF_FRAME_LENGTH_ADDRESS + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_MSP_PAYLOAD_SIZE, crsfFrame.frame.frameLength);
    EXPECT_EQ(CRSF_FRAMETYPE_MSP_REQ, crsfFrame.frame.type);
    //Sconst uint8_t crc = crsfFrameCRC();
}

// STUBS

extern "C" {

    gpsSolutionData_t gpsSol;
    attitudeEulerAngles_t attitude = { { 0, 0, 0 } };

    uint32_t micros(void) {return dummyTimeUs;}
    serialPort_t *openSerialPort(serialPortIdentifier_e, serialPortFunction_e, serialReceiveCallbackPtr, uint32_t, portMode_e, portOptions_e) {return NULL;}
    serialPortConfig_t *findSerialPortConfig(serialPortFunction_e ) {return NULL;}
    uint16_t getBatteryVoltage(void) {
        return testBatteryVoltage;
    }
    int32_t getAmperage(void) {
        return testAmperage;
    }

    uint8_t calculateBatteryPercentageRemaining(void) {
        return 67;
    }

    bool feature(uint32_t) {return false;}

    bool isAirmodeActive(void) {return true;}

    mspResult_e mspFcProcessCommand(mspPacket_t *, mspPacket_t *, mspPostProcessFnPtr *) {
        return MSP_RESULT_ACK;
    }

    void beeperConfirmationBeeps(uint8_t ) {}
}
