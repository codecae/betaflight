#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "platform.h"

#ifdef TELEMETRY

#include "build/build_config.h"

#include "common/utils.h"

#include "fc/fc_msp.h"

#include "msp/msp.h"

#include "rx/crsf.h"
#include "rx/msp.h"

#include "telemetry/msp_shared.h"
#include "telemetry/smartport.h"

#define TELEMETRY_MSP_VERSION    1
#define TELEMETRY_MSP_VER_SHIFT  5
#define TELEMETRY_MSP_VER_MASK   (0x7 << TELEMETRY_MSP_VER_SHIFT)
#define TELEMETRY_MSP_ERROR_FLAG (1 << 5)
#define TELEMETRY_MSP_START_FLAG (1 << 4)
#define TELEMETRY_MSP_SEQ_MASK   0x0F
#define TELEMETRY_MSP_RES_ERROR (-10)

enum {
    TELEMETRY_MSP_VER_MISMATCH=0,
    TELEMETRY_MSP_CRC_ERROR=1,
    TELEMETRY_MSP_ERROR=2
};

#define REQUEST_BUFFER_SIZE 64
#define RESPONSE_BUFFER_SIZE 64
    
STATIC_UNIT_TESTED uint8_t checksum = 0;
STATIC_UNIT_TESTED mspPackage_t mspPackage;
static mspRxBuffer_t mspRxBuffer;
static mspTxBuffer_t mspTxBuffer;
static mspPacket_t mspRxPacket;
static mspPacket_t mspTxPacket;

void initSharedMsp() {
    mspPackage.requestBuffer = (uint8_t *)&mspRxBuffer;
    mspPackage.requestPacket = &mspRxPacket;
    mspPackage.requestPacket->buf.ptr = mspPackage.requestBuffer;
    mspPackage.requestPacket->buf.end = mspPackage.requestBuffer;

    mspPackage.responseBuffer = (uint8_t *)&mspTxBuffer;
    mspPackage.responsePacket = &mspTxPacket;
    mspPackage.responsePacket->buf.ptr = mspPackage.responseBuffer;
    mspPackage.responsePacket->buf.end = mspPackage.responseBuffer;
}

static void processMspPacket()
{
    mspPackage.responsePacket->cmd = 0;
    mspPackage.responsePacket->result = 0;
    mspPackage.responsePacket->buf.end = mspPackage.responseBuffer;

    mspPostProcessFnPtr mspPostProcessFn = NULL;
    if (mspFcProcessCommand(mspPackage.requestPacket, mspPackage.responsePacket, &mspPostProcessFn) == MSP_RESULT_ERROR) {
        sbufWriteU8(&mspPackage.responsePacket->buf, TELEMETRY_MSP_ERROR);
    }
    if (mspPostProcessFn) {
        mspPostProcessFn(NULL);
    }

    sbufSwitchToReader(&mspPackage.responsePacket->buf, mspPackage.responseBuffer);
}

void sendMspErrorResponse(uint8_t error, int16_t cmd)
{
    mspPackage.responsePacket->cmd = cmd;
    mspPackage.responsePacket->result = 0;    
    mspPackage.responsePacket->buf.end = mspPackage.responseBuffer;

    sbufWriteU8(&mspPackage.responsePacket->buf, error);
    mspPackage.responsePacket->result = TELEMETRY_MSP_RES_ERROR;
    sbufSwitchToReader(&mspPackage.responsePacket->buf, mspPackage.responseBuffer);
}

bool handleMspFrame(uint8_t *frameStart, uint8_t *frameEnd)
{
    static uint8_t mspStarted = 0;
    static uint8_t lastSeq = 0;

    mspPacket_t *packet = mspPackage.requestPacket;
    sbuf_t *frameBuf = &mspPackage.requestFrame;

    if (sbufBytesRemaining(&mspPackage.responsePacket->buf) > 0) {
        return false;
    } 
      
    if (mspStarted == 0) {
        initSharedMsp();
    }
    
    uint8_t frameLength = frameEnd - frameStart;
    uint8_t frame[frameLength];

    memcpy(&frame, frameStart, frameLength);

    frameBuf->ptr = (uint8_t*)&frame;
    frameBuf->end = (uint8_t*)&frame + frameLength;

    uint8_t header = sbufReadU8(frameBuf);
    uint8_t seqNumber = header & TELEMETRY_MSP_SEQ_MASK; 
    uint8_t version = (header & TELEMETRY_MSP_VER_MASK) >> TELEMETRY_MSP_VER_SHIFT;

    if (version != TELEMETRY_MSP_VERSION) {
        sendMspErrorResponse(TELEMETRY_MSP_VER_MISMATCH, 0);
        return true;
    }

    if (header & TELEMETRY_MSP_START_FLAG) {
        // first packet in sequence
        uint8_t mspPayloadSize = sbufReadU8(frameBuf);
        
        packet->cmd = sbufReadU8(frameBuf);
        packet->result = 0;
        packet->buf.ptr = mspPackage.requestBuffer;
        packet->buf.end = mspPackage.requestBuffer + mspPayloadSize;

        checksum = mspPayloadSize ^ packet->cmd;
        mspStarted = 1;
    } else if (!mspStarted) {
        // no start packet yet, throw this one away
        return false;
    } else if (((lastSeq + 1) & TELEMETRY_MSP_SEQ_MASK) != seqNumber) {
        // packet loss detected!
        mspStarted = 0;
        return false; 
    }

    while ((frameBuf->ptr < frameBuf->end) && sbufBytesRemaining(&packet->buf)) {
        checksum ^= *frameBuf->ptr;
        sbufWriteU8(&packet->buf, sbufReadU8(frameBuf));
    }

    if (sbufBytesRemaining(frameBuf) == 0) {
        lastSeq = seqNumber;
        return false;
    }

    if (checksum != *frameBuf->ptr) {
        mspStarted = 0;
        sendMspErrorResponse(TELEMETRY_MSP_CRC_ERROR, packet->cmd);
        return true;
    } 

    mspStarted = 0;
    sbufSwitchToReader(&packet->buf, mspPackage.requestBuffer);
    processMspPacket();
    return true;
}

bool sendMspReply(uint8_t payloadSize, mspResponseFnPtr responseFn)
{
    static uint8_t checksum = 0;
    static uint8_t seq = 0;

    uint8_t packet[payloadSize];
    uint8_t *p = packet;
    uint8_t *end = p + payloadSize;

    sbuf_t *txBuf = &mspPackage.responsePacket->buf;

    // detect first reply packet
    if (txBuf->ptr == mspPackage.responseBuffer) {

        // header
        uint8_t head = TELEMETRY_MSP_START_FLAG | (seq++ & TELEMETRY_MSP_SEQ_MASK);
        if (mspPackage.responsePacket->result < 0) {
            head |= TELEMETRY_MSP_ERROR_FLAG;
        }
        *p++ = head;

        uint8_t size = sbufBytesRemaining(txBuf);
        *p++ = size;

        checksum = size ^ mspPackage.responsePacket->cmd;
    }
    else {
        // header
        *p++ = (seq++ & TELEMETRY_MSP_SEQ_MASK);
    }

    while ((p < end) && (sbufBytesRemaining(txBuf) > 0)) {
        *p = sbufReadU8(txBuf);
        checksum ^= *p++; // MSP checksum
    }

    // to be continued...
    if (p == end) {
        responseFn(packet);
        return true;
    }

    // nothing left in txBuf,
    // append the MSP checksum
    *p++ = checksum;

    // pad with zeros
    while (p < end)
        *p++ = 0;
        responseFn(packet);
    return false;
}

#endif