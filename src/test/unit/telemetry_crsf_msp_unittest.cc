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
    #include "common/streambuf.h"
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
    #include "telemetry/smartport.h"

    bool handleMspFrame(uint8_t *frameStart, uint8_t *frameEnd);
    bool sendMspReply(uint8_t payloadSize, mspResponseFnPtr responseFn);
    uint8_t sbufReadU8(sbuf_t *src);
    int sbufBytesRemaining(sbuf_t *buf);
    void initSharedMsp();
    uint16_t testBatteryVoltage = 0;
    int32_t testAmperage = 0;
    uint8_t mspTxData[64]; //max frame size
    sbuf_t mspTxDataBuf;
    uint8_t crsfFrameOut[CRSF_FRAME_SIZE_MAX];

    PG_REGISTER(batteryConfig_t, batteryConfig, PG_BATTERY_CONFIG, 0);
    PG_REGISTER(telemetryConfig_t, telemetryConfig, PG_TELEMETRY_CONFIG, 0);
    PG_REGISTER(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG, 0);

    extern bool crsfFrameDone;
    extern crsfFrame_t crsfFrame;
    extern mspPackage_t mspPackage;
  
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

TEST(CrossFireMSPTest, RequestBufferTest)
{
    initSharedMsp();
    const crsfMspFrame_t *framePtr = (const crsfMspFrame_t*)crsfPidRequest;
    crsfFrame = *(const crsfFrame_t*)framePtr;
    crsfFrameDone = true;
    EXPECT_EQ(CRSF_ADDRESS_BROADCAST, crsfFrame.frame.deviceAddress);
    EXPECT_EQ(CRSF_FRAME_LENGTH_ADDRESS + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_MSP_PAYLOAD_SIZE, crsfFrame.frame.frameLength);
    EXPECT_EQ(CRSF_FRAMETYPE_MSP_REQ, crsfFrame.frame.type);
    uint8_t *destination = (uint8_t *)&crsfFrame.frame.payload; 
    uint8_t *origin = (uint8_t *)&crsfFrame.frame.payload + 1;
    uint8_t *frameStart = (uint8_t *)&crsfFrame.frame.payload + 2;
    uint8_t *frameEnd = (uint8_t *)&crsfFrame.frame.payload + CRSF_FRAME_MSP_PAYLOAD_SIZE + 1;
    EXPECT_EQ(0xC8, *destination);
    EXPECT_EQ(0xEA, *origin);
    EXPECT_EQ(0x30, *frameStart);
    EXPECT_EQ(0x00, *frameEnd);
    handleMspFrame(frameStart, frameEnd);
    sbufSwitchToReader(&mspPackage.requestPacket->buf, mspPackage.requestBuffer);
    EXPECT_EQ(0x30, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x00, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x70, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x70, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x00, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x00, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x00, sbufReadU8(&mspPackage.requestPacket->buf));
    EXPECT_EQ(0x00, sbufReadU8(&mspPackage.requestPacket->buf));
    sbufSwitchToReader(&mspPackage.requestPacket->buf, mspPackage.requestBuffer);
    EXPECT_EQ(8, sbufBytesRemaining(&mspPackage.requestPacket->buf));

}

TEST(CrossFireMSPTest, ResponsePacketTest)
{
    initSharedMsp();
    const crsfMspFrame_t *framePtr = (const crsfMspFrame_t*)crsfPidRequest;
    crsfFrame = *(const crsfFrame_t*)framePtr;
    crsfFrameDone = true;
    uint8_t *frameStart = (uint8_t *)&crsfFrame.frame.payload + 2;
    uint8_t *frameEnd = (uint8_t *)&crsfFrame.frame.payload + CRSF_FRAME_MSP_PAYLOAD_SIZE + 1;
    handleMspFrame(frameStart, frameEnd);
    for (unsigned int ii=1; ii<30; ii++) {
        EXPECT_EQ(ii, sbufReadU8(&mspPackage.responsePacket->buf));
    }
    sbufSwitchToReader(&mspPackage.responsePacket->buf, mspPackage.responseBuffer);
}

TEST(CrossFireMSPTest, WriteResponseTest)
{
    initSharedMsp();

}

/*void crsfSendMspResponse(uint8_t *packet) 
{
    sbuf_t mspPayload;
    sbuf_t *dst = &mspTxDataBuf;
    sbuf_t *msp = &mspPayload;

    msp->ptr = packet;
    msp->end = packet + CRSF_FRAME_MSP_PAYLOAD_SIZE;
    dst->ptr = crsfFrameOut;
    dst->end = ARRAYEND(crsfFrameOut);

    sbufWriteU8(dst, CRSF_ADDRESS_BROADCAST);
    sbufWriteU8(dst, CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_MSP_RESP);
    sbufWriteU8(dst, CRSF_ADDRESS_RADIO_TRANSMITTER);
    sbufWriteU8(dst, CRSF_ADDRESS_BETAFLIGHT);
    while (sbufBytesRemaining(msp)) {
        sbufWriteU8(dst, sbufReadU8(msp));
    }
    //skip crc for testing
    sbufSwitchToReader(dst, crsfFrameOut);
 }

TEST(CrossFireMSPTest, ResponseBufferTest_Small) {
    initSharedMsp();
    const crsfMspFrame_t *framePtr = (const crsfMspFrame_t*)crsfPidRequest;
    crsfFrame = *(const crsfFrame_t*)framePtr;
    crsfFrameDone = true;
    uint8_t *frameStart = (uint8_t *)&crsfFrame.frame.payload + 2;
    uint8_t *frameEnd = (uint8_t *)&crsfFrame.frame.payload + CRSF_FRAME_MSP_PAYLOAD_SIZE + 2;
    handleMspFrame(frameStart, frameEnd);
    bool pending1 = sendMspReply(CRSF_FRAME_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
    EXPECT_TRUE(pending1); 
    EXPECT_EQ(0x00, crsfFrameOut[0]); // broadcast
    EXPECT_EQ(CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC, crsfFrameOut[1]); // crsf frame length
    EXPECT_EQ(0x7B, crsfFrameOut[2]); // response
    EXPECT_EQ(0xEA, crsfFrameOut[3]); // to the transmitter
    EXPECT_EQ(0xC8, crsfFrameOut[4]); // from betaflight
    EXPECT_EQ(0x10, crsfFrameOut[5]); // first frame
    EXPECT_EQ(0x1E, crsfFrameOut[6]); // length of transport (30)
    EXPECT_EQ(1, crsfFrameOut[7]); // msp data begins
    EXPECT_EQ(2, crsfFrameOut[8]); // ...
    EXPECT_EQ(3, crsfFrameOut[9]); // ...
    EXPECT_EQ(4, crsfFrameOut[10]); // ...
    EXPECT_EQ(5, crsfFrameOut[11]); // ...
    EXPECT_EQ(6, crsfFrameOut[12]); // ...
    // Skip CRC
    // NEXT FRAME
    bool pending2 = sendMspReply(CRSF_FRAME_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
    EXPECT_TRUE(pending2); 
    EXPECT_EQ(0x00, crsfFrameOut[0]); // broadcast
    EXPECT_EQ(CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC, crsfFrameOut[1]); // crsf frame length
    EXPECT_EQ(0x7B, crsfFrameOut[2]); // response
    EXPECT_EQ(0xEA, crsfFrameOut[3]); // to the transmitter
    EXPECT_EQ(0xC8, crsfFrameOut[4]); // from betaflight
    EXPECT_EQ(0x01, crsfFrameOut[5]); // second frame
    EXPECT_EQ(7, crsfFrameOut[6]); // ... 
    EXPECT_EQ(8, crsfFrameOut[7]); // ... 
    EXPECT_EQ(9, crsfFrameOut[8]); // ... 
    EXPECT_EQ(10, crsfFrameOut[9]); // ... 
    EXPECT_EQ(11, crsfFrameOut[10]); // ... 
    EXPECT_EQ(12, crsfFrameOut[11]); // ... 
    EXPECT_EQ(13, crsfFrameOut[12]); // ... 
    // Skip CRC
    // NEXT FRAME
    bool pending3 = sendMspReply(CRSF_FRAME_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
    EXPECT_TRUE(pending3); 
    EXPECT_EQ(0x00, crsfFrameOut[0]); // broadcast
    EXPECT_EQ(CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC, crsfFrameOut[1]); // crsf frame length
    EXPECT_EQ(0x7B, crsfFrameOut[2]); // response
    EXPECT_EQ(0xEA, crsfFrameOut[3]); // to the transmitter
    EXPECT_EQ(0xC8, crsfFrameOut[4]); // from betaflight
    EXPECT_EQ(0x02, crsfFrameOut[5]); // third frame
    EXPECT_EQ(14, crsfFrameOut[6]); // ... 
    EXPECT_EQ(15, crsfFrameOut[7]); // ... 
    EXPECT_EQ(16, crsfFrameOut[8]); // ... 
    EXPECT_EQ(17, crsfFrameOut[9]); // ... 
    EXPECT_EQ(18, crsfFrameOut[10]); // ... 
    EXPECT_EQ(19, crsfFrameOut[11]); // ... 
    EXPECT_EQ(20, crsfFrameOut[12]); // ... 
    // Skip CRC
    // NEXT FRAME
    bool pending4 = sendMspReply(CRSF_FRAME_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
    EXPECT_TRUE(pending4); 
    EXPECT_EQ(0x00, crsfFrameOut[0]); // broadcast
    EXPECT_EQ(CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC, crsfFrameOut[1]); // crsf frame length
    EXPECT_EQ(0x7B, crsfFrameOut[2]); // response
    EXPECT_EQ(0xEA, crsfFrameOut[3]); // to the transmitter
    EXPECT_EQ(0xC8, crsfFrameOut[4]); // from betaflight
    EXPECT_EQ(0x03, crsfFrameOut[5]); // fourth frame
    EXPECT_EQ(21, crsfFrameOut[6]); // ... 
    EXPECT_EQ(22, crsfFrameOut[7]); // ... 
    EXPECT_EQ(23, crsfFrameOut[8]); // ... 
    EXPECT_EQ(24, crsfFrameOut[9]); // ... 
    EXPECT_EQ(25, crsfFrameOut[10]); // ... 
    EXPECT_EQ(26, crsfFrameOut[11]); // ... 
    EXPECT_EQ(27, crsfFrameOut[12]); // ... 
    // Skip CRC
    // NEXT FRAME
    bool pending5 = sendMspReply(CRSF_FRAME_MSP_PAYLOAD_SIZE, &crsfSendMspResponse);
    EXPECT_FALSE(pending5);  // LAST FRAME!
    EXPECT_EQ(0x00, crsfFrameOut[0]); // broadcast
    EXPECT_EQ(CRSF_FRAME_MSP_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_EXT_TYPE_CRC, crsfFrameOut[1]); // crsf frame length
    EXPECT_EQ(0x7B, crsfFrameOut[2]); // response
    EXPECT_EQ(0xEA, crsfFrameOut[3]); // to the transmitter
    EXPECT_EQ(0xC8, crsfFrameOut[4]); // from betaflight
    EXPECT_EQ(0x04, crsfFrameOut[5]); // fourth frame
    EXPECT_EQ(28, crsfFrameOut[6]); // ... 
    EXPECT_EQ(29, crsfFrameOut[7]); // ... 
    EXPECT_EQ(30, crsfFrameOut[8]); // ... 
    EXPECT_EQ(0x71, crsfFrameOut[9]); // ... MSP CRC
    EXPECT_EQ(0, crsfFrameOut[10]); // ... 
    EXPECT_EQ(0, crsfFrameOut[11]); // ... 
    EXPECT_EQ(0, crsfFrameOut[12]); // ... 
    // Skip CRC
}*/


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

    mspResult_e mspFcProcessCommand(mspPacket_t *cmd, mspPacket_t *reply, mspPostProcessFnPtr *mspPostProcessFn) {

        UNUSED(mspPostProcessFn);

        sbuf_t *dst = &reply->buf;
        sbuf_t *src = &cmd->buf;
        const uint8_t cmdMSP = cmd->cmd;
        reply->cmd = cmd->cmd;

        if (cmdMSP == 0x70) {
            for (unsigned int ii=1; ii<=30; ii++) {
                sbufWriteU8(dst, ii);
            }
        } elseif (cmdMSP = 0xCA) {
            return MSP_RESULT_ACK;
        }

        return MSP_RESULT_ACK;
    }

    void beeperConfirmationBeeps(uint8_t ) {}
}
