/*
 * Telemetry MSP wrapper
 * Decoupled from smartport.c for use in additional protocols
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "platform.h"

#ifdef TELEMETRY

#include "common/utils.h"

#include "fc/fc_msp.h"

#include "msp/msp.h"

#include "telemetry/telem_msp.h"

#define TELEMETRY_MSP_VERSION    1
#define TELEMETRY_MSP_VER_SHIFT  5
#define TELEMETRY_MSP_VER_MASK   (0x7 << TELEMETRY_MSP_VER_SHIFT)
#define TELEMETRY_MSP_VERSION_S  (TELEMETRY_MSP_VERSION << TELEMETRY_MSP_VER_SHIFT)
#define TELEMETRY_MSP_ERROR_FLAG (1 << 5)
#define TELEMETRY_MSP_START_FLAG (1 << 4)
#define TELEMETRY_MSP_SEQ_MASK   0x0F
#define TELEMETRY_MSP_RX_BUF_SIZE 64
#define TELEMETRY_MSP_RES_ERROR (-10)

enum {
    TELEMETRY_MSP_VER_MISMATCH=0,
    TELEMETRY_MSP_CRC_ERROR=1,
    TELEMETRY_MSP_ERROR=2
};

static void initTelemetryMspReply(int16_t cmd, mspPacket_t *replyPacket, uint8_t *txBuffer)
{
    replyPacket->buf.ptr = txBuffer;
    replyPacket->buf.end = ARRAYEND(txBuffer);
    replyPacket->cmd = cmd;
    replyPacket->result = 0;
}

void processMspPacket(mspPacket_t *packet, mspPacket_t *replyPacket, uint8_t *txBuffer)
{
    initTelemetryMspReply(0, replyPacket, txBuffer);

    mspPostProcessFnPtr mspPostProcessFn = NULL;
    if (mspFcProcessCommand(packet, replyPacket, &mspPostProcessFn) == MSP_RESULT_ERROR) {
        sbufWriteU8(&replyPacket->buf, TELEMETRY_MSP_ERROR);
    }
    if (mspPostProcessFn) {
        mspPostProcessFn(NULL);
    }

    sbufSwitchToReader(&replyPacket->buf, txBuffer);
}

void generateMspErrorReply(uint8_t error, int16_t cmd, mspPacket_t *replyPacket, uint8_t *txBuffer)
{
    initTelemetryMspReply(cmd, replyPacket, txBuffer);
    sbufWriteU8(&replyPacket->buf,error);
    replyPacket->result = TELEMETRY_MSP_RES_ERROR;

    sbufSwitchToReader(&replyPacket->buf, txBuffer);
}


/**
 * Request frame format:
 * - Header: 1 byte
 *   - Reserved: 2 bits (future use)
 *   - Error-flag: 1 bit
 *   - Start-flag: 1 bit
 *   - CSeq: 4 bits
 *
 * - MSP payload:
 *   - if Error-flag == 0:
 *     - size: 1 byte
 *     - payload
 *     - CRC (request type included)
 *   - if Error-flag == 1:
 *     - size: 1 byte (== 1)
 *     - error: 1 Byte
 *       - 0: Version mismatch (type=0)
 *       - 1: Sequence number error
 *       - 2: MSP error
 *     - CRC (request type included)
 */

bool sendTelemetryMspReply(uint8_t payloadSize, mspPacket_t *replyPacket, uint8_t *txBuffer, sendMspTelemetryFnPtr sendMspTelemetryFn)
{
    static uint8_t checksum = 0;
    static uint8_t seq = 0;

    uint8_t packet[payloadSize];
    uint8_t* p = packet;
    uint8_t* end = p + payloadSize;

    sbuf_t* txBuf = &replyPacket->buf;

    // detect first reply packet
    if (txBuf->ptr == txBuffer) {

        // header
        uint8_t head = TELEMETRY_MSP_START_FLAG | (seq++ & TELEMETRY_MSP_SEQ_MASK);
        if (replyPacket->result < 0) {
            head |= TELEMETRY_MSP_ERROR_FLAG;
        }
        *p++ = head;

        uint8_t size = sbufBytesRemaining(txBuf);
        *p++ = size;

        checksum = size ^ replyPacket->cmd;
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
        //smartPortSendPackageEx(FSSP_MSPS_FRAME,packet);
        if(sendMspTelemetryFn) {
            sendMspTelemetryFn(packet);
        } 
        return true;
    }

    // nothing left in txBuf,
    // append the MSP checksum
    *p++ = checksum;

    // pad with zeros
    while (p < end)
        *p++ = 0;

    //smartPortSendPackageEx(FSSP_MSPS_FRAME,packet);
    if(sendMspTelemetryFn) {
        sendMspTelemetryFn(packet);
    } 
    return false;
}

#endif