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

#include <string.h>
#include <stdbool.h>

#include "platform.h"
#include "common/time.h"
#include "common/utils.h"
#include "interface/tramp_protocol.h"

#define TRAMP_PAYLOAD_LENGTH        12
#define TRAMP_SYNC_START            0x0F
#define TRAMP_SYNC_STOP             0x00
#define TRAMP_COMMAND_SET_FREQ      'F' // 0x46
#define TRAMP_COMMAND_SET_POWER     'P' // 0x50
#define TRAMP_COMMAND_ACTIVE_STATE  'I' // 0x49
#define TRAMP_COMMAND_GET_CONFIG    'v' // 0x76

typedef struct trampFrameHeader_s {
    uint8_t syncStart;
    uint8_t command;
} __attribute__((packed)) trampFrameHeader_t;

#define TRAMP_HEADER_LENGTH sizeof(trampFrameHeader_t)

typedef struct trampFrameFooter_s {
    uint8_t crc;
    uint8_t syncStop;
} __attribute__((packed)) trampFrameFooter_t;

typedef union trampPayload_u {
    uint8_t buf[TRAMP_PAYLOAD_LENGTH];
    vtxDeviceSettings_t settings;
    uint16_t frequency;
    uint16_t power;
    uint8_t active;
} trampPayload_t;

typedef struct trampFrame_s {
    trampFrameHeader_t header;
    trampPayload_t payload;
    trampFrameFooter_t footer;
} __attribute__((packed)) trampFrame_t;

STATIC_ASSERT(sizeof(trampFrameHeader_t) == 2, trampInterface_headerSizeMismatch);
STATIC_ASSERT(sizeof(trampFrame_t) == TRAMP_FRAME_LENGTH, trampInterface_frameSizeMismatch);

static uint8_t trampCrc(const trampFrame_t *frame)
{
    uint8_t crc = 0;
    const uint8_t *p = (const uint8_t *)frame;
    const uint8_t *pEnd = p + (TRAMP_HEADER_LENGTH + TRAMP_PAYLOAD_LENGTH);
    for (; p != pEnd; p++) {
        crc += *p;
    }
    return crc;
}

static void trampFrameInit(uint8_t frameType, trampFrame_t *frame)
{
    frame->header.syncStart = TRAMP_SYNC_START;
    frame->header.command = frameType;
    const uint8_t emptyPayload[TRAMP_PAYLOAD_LENGTH] = { 0 };
    memcpy(frame->payload.buf, emptyPayload, sizeof(frame->payload.buf));
}

static void trampFrameClose(trampFrame_t *frame)
{
    frame->footer.crc = trampCrc(frame);
    frame->footer.syncStop = TRAMP_SYNC_STOP;
}

bool trampFrameGetSettings(uint8_t *buf, size_t bufLen)
{
    if (bufLen != TRAMP_FRAME_LENGTH) {
        return false;
    }
    trampFrame_t *frame = (trampFrame_t *)buf;
    trampFrameInit(TRAMP_COMMAND_GET_CONFIG, frame);
    trampFrameClose(frame);
    return true;
}

bool trampFrameSetFrequency(uint8_t *buf, size_t bufLen, const uint16_t frequency)
{
    if (bufLen != TRAMP_FRAME_LENGTH) {
        return false;
    }
    trampFrame_t *frame = (trampFrame_t *)buf;
    trampFrameInit(TRAMP_COMMAND_SET_FREQ, frame);
    frame->payload.frequency = (frequency << 8) | ((frequency >> 8) & 0xFF);
    trampFrameClose(frame);
    return true;
}

bool trampFrameSetPower(uint8_t *buf, size_t bufLen, const uint16_t power)
{
    if (bufLen != TRAMP_FRAME_LENGTH) {
        return false;
    }
    trampFrame_t *frame = (trampFrame_t *)buf;
    trampFrameInit(TRAMP_COMMAND_SET_POWER, frame);
    frame->payload.power = power;
    trampFrameClose(frame);
    return true;
}

bool trampFrameSetActiveState(uint8_t *buf, size_t bufLen, const bool active)
{
    if (bufLen != TRAMP_FRAME_LENGTH) {
        return false;
    }
    trampFrame_t *frame = (trampFrame_t *)buf;
    trampFrameInit(TRAMP_COMMAND_ACTIVE_STATE, frame);
    frame->payload.active = (uint8_t) active;
    trampFrameClose(frame);
    return true;
}

bool trampParseResponseBuffer(vtxDeviceSettings_t *settings, const uint8_t *buf, size_t bufLen)
{
    if (bufLen != TRAMP_FRAME_LENGTH) {
        return false;
    }
    const trampFrame_t *frame = (const trampFrame_t *)buf;
    const uint8_t crc = trampCrc(frame);
    if (crc != frame->footer.crc) {
        return false;
    }
    memcpy(settings, &frame->payload.settings, sizeof(*settings));
    settings->frequency = (settings->frequency << 8) | ((settings->frequency >> 8) & 0xFF);
    return true;
}
