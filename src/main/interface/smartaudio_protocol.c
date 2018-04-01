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

#include "platform.h"
#include "common/crc.h"
#include "interface/smartaudio_protocol.h"

#define SMARTAUDIO_SYNC_BYTE            0xAA
#define SMARTAUDIO_HEADER_BYTE          0x55
#define SMARTAUDIO_START_CODE           SMARTAUDIO_SYNC_BYTE + SMARTAUDIO_HEADER_BYTE
#define SMARTAUDIO_GET_PITMODE_FREQ     (1 << 14)
#define SMARTAUDIO_SET_PITMODE_FREQ     (1 << 15)
#define SMARTAUDIO_FREQUENCY_MASK       0x3FFF

#define SMARTAUDIO_CMD_GET_SETTINGS     0x03
#define SMARTAUDIO_CMD_SET_POWER        0x05
#define SMARTAUDIO_CMD_SET_CHANNEL      0x07
#define SMARTAUDIO_CMD_SET_FREQUENCY    0x09
#define SMARTAUDIO_CMD_SET_MODE         0x0B

#define SMARTAUDIO_RSP_GET_SETTINGS_V1  SMARTAUDIO_CMD_GET_SETTINGS >> 1
#define SMARTAUDIO_RSP_GET_SETTINGS_V2  (SMARTAUDIO_CMD_GET_SETTINGS >> 1) | 0x08
#define SMARTAUDIO_RSP_SET_POWER        SMARTAUDIO_CMD_SET_POWER >> 1
#define SMARTAUDIO_RSP_SET_CHANNEL      SMARTAUDIO_CMD_SET_CHANNEL >> 1
#define SMARTAUDIO_RSP_SET_FREQUENCY    SMARTAUDIO_CMD_SET_FREQUENCY >> 1
#define SMARTAUDIO_RSP_SET_MODE         SMARTAUDIO_CMD_SET_MODE >> 1

#define SMARTAUDIO_BANDCHAN_TO_INDEX(band, channel) (band * 8 + (channel))
#define U16BIGENDIAN(bytes) (bytes << 8) | ((bytes >> 8) & 0xFF)

typedef struct smartAudioFrameHeader_s {
    uint16_t startCode;
    uint8_t length;
    uint8_t command;
} __attribute__((packed)) smartAudioFrameHeader_t;

typedef struct smartAudioCommandOnlyFrame_s {
    smartAudioFrameHeader_t header;
    uint8_t crc;
} __attribute__((packed)) smartAudioCommandOnlyFrame_t;

typedef struct smartAudioU8Frame_s {
    smartAudioFrameHeader_t header;
    uint8_t payload;
    uint8_t crc;
} __attribute__((packed)) smartAudioU8Frame_t;

typedef struct smartaudioU16Frame_s {
    smartAudioFrameHeader_t header;
    uint16_t payload;
    uint8_t crc;
} __attribute__((packed)) smartAudioU16Frame_t;

typedef struct smartAudioU8ResponseFrame_s {
    smartAudioFrameHeader_t header;
    uint8_t payload;
    uint8_t reserved;
    uint8_t crc;
} __attribute__((packed)) smartAudioU8ResponseFrame_t;

typedef struct smartAudioU16ResponseFrame_s {
    smartAudioFrameHeader_t header;
    uint16_t payload;
    uint8_t reserved;
    uint8_t crc;
} __attribute__((packed)) smartAudioU16ResponseFrame_t;

typedef struct smartAudioSettingsResponseFrame_s {
    smartAudioFrameHeader_t header;
    uint8_t channel;
    uint8_t power;
    uint8_t operationMode;
    uint16_t frequency;
    uint8_t crc;
} __attribute__((packed)) smartAudioSettingsResponseFrame_t;

static const uint8_t smartAudioPowerLevelTranslation[SMARTAUDIO_POWER_LEVELS] = { 7, 16, 25, 40 };  // v2 index to v1 DAC value translation

static uint8_t smartAudioPowerLevelToDacValue(const uint8_t powerLevel) {
    if (powerLevel < SMARTAUDIO_POWER_LEVELS) {
        return smartAudioPowerLevelTranslation[powerLevel];
    }
    return smartAudioPowerLevelTranslation[0];
}

static uint8_t smartAudioDacValueToPowerLevel(const uint8_t dacValue) {
    for (int i = 0; i < SMARTAUDIO_POWER_LEVELS; i++) {
        if (smartAudioPowerLevelTranslation[i] >= dacValue) {
            return i;
        }
    }
    return 0;
}

static void smartAudioFrameInit(const uint8_t command, smartAudioFrameHeader_t *header, const uint8_t payloadLength)
{
    header->startCode = SMARTAUDIO_START_CODE;
    header->length = payloadLength;
    header->command = command;
}

static void smartAudioUnpackOperationMode(vtxDeviceSettings_t *settings, const uint8_t operationMode, const bool settingsResponse)
{
    if (settingsResponse) {
        // operation mode bit order is different between 'Get Settings' and 'Set Mode' responses.
        settings->userFrequencyMode = operationMode & 0x01;
        settings->pitModeEnabled = operationMode & 0x02;
        settings->pitModeInRangeActive = operationMode & 0x04;
        settings->pitModeOutRangeActive = operationMode & 0x08;
        settings->unlocked = operationMode & 0x10;
    } else {
        settings->pitModeInRangeActive = operationMode & 0x01;
        settings->pitModeOutRangeActive = operationMode & 0x02;
        settings->pitModeEnabled = operationMode & 0x04;
        settings->unlocked = operationMode & 0x08;
    }
}

static void smartAudioUnpackFrequency(vtxDeviceSettings_t *settings, const uint16_t frequency)
{
    if (frequency & SMARTAUDIO_GET_PITMODE_FREQ) {
        settings->pitModeFrequency = U16BIGENDIAN(frequency & SMARTAUDIO_FREQUENCY_MASK);
    } else {
        settings->frequency = U16BIGENDIAN(frequency & SMARTAUDIO_FREQUENCY_MASK);
    }
}

static void smartAudioUnpackSettings(vtxDeviceSettings_t *settings, const smartAudioSettingsResponseFrame_t *frame)
{
    settings->channel = frame->channel;
    settings->power = frame->power;
    smartAudioUnpackFrequency(settings, frame->frequency);
    smartAudioUnpackOperationMode(settings, frame->operationMode, true);
}

static uint8_t smartAudioPackOperationMode(const vtxDeviceSettings_t *settings)
{
    uint8_t operationMode = 0;
    operationMode |= settings->pitModeInRangeActive << 0;
    operationMode |= settings->pitModeOutRangeActive << 1;
    operationMode |= settings->pitModeEnabled << 2;
    operationMode |= settings->unlocked << 3;
    return operationMode;
}

size_t smartAudioFrameGetSettings(uint8_t *buf, size_t bufLen)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioCommandOnlyFrame_t *frame = (smartAudioCommandOnlyFrame_t *)buf;
    smartAudioFrameInit(SMARTAUDIO_CMD_GET_SETTINGS, &frame->header, 0);
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioCommandOnlyFrame_t) - sizeof(frame->crc));
    return sizeof(smartAudioCommandOnlyFrame_t);
}

size_t smartaudioFrameGetPitModeFrequency(uint8_t *buf, size_t bufLen)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioU16Frame_t *frame = (smartAudioU16Frame_t *)buf;
    smartAudioFrameInit(SMARTAUDIO_CMD_SET_FREQUENCY, &frame->header, sizeof(frame->payload));
    frame->payload = SMARTAUDIO_SET_PITMODE_FREQ;
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioU16Frame_t) - sizeof(frame->crc));
    return sizeof(smartAudioU16Frame_t);
}

size_t smartAudioFrameSetPower(uint8_t *buf, size_t bufLen, const int version, const uint8_t powerLevel)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioU8Frame_t *frame = (smartAudioU8Frame_t *)buf;
    switch (version) {
        case SMARTAUDIO_VERSION_1:
            frame->payload = smartAudioPowerLevelToDacValue(powerLevel);
            break;
        case SMARTAUDIO_VERSION_2:
            frame->payload = powerLevel;
            break;
        default:
            frame->payload = 0;
            break;
    }
    smartAudioFrameInit(SMARTAUDIO_CMD_SET_POWER, &frame->header, sizeof(frame->payload));
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioU8Frame_t) - sizeof(frame->crc));
    return sizeof(smartAudioU8Frame_t);
}

size_t smartAudioFrameSetBandChannel(uint8_t *buf, size_t bufLen, const uint8_t band, const uint8_t channel)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioU8Frame_t *frame = (smartAudioU8Frame_t *)buf;
    smartAudioFrameInit(SMARTAUDIO_CMD_SET_CHANNEL, &frame->header, sizeof(frame->payload));
    frame->payload = SMARTAUDIO_BANDCHAN_TO_INDEX(band, channel);
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioU8Frame_t) - sizeof(frame->crc));
    return sizeof(smartAudioU8Frame_t);
}

size_t smartaudioFrameSetFrequency(uint8_t *buf, size_t bufLen, const uint16_t frequency, const bool pitModeFrequency)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioU16Frame_t *frame = (smartAudioU16Frame_t *)buf;
    smartAudioFrameInit(SMARTAUDIO_CMD_SET_FREQUENCY, &frame->header, sizeof(frame->payload));
    frame->payload = U16BIGENDIAN(frequency | (pitModeFrequency ? SMARTAUDIO_SET_PITMODE_FREQ : 0x00));
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioU16Frame_t) - sizeof(frame->crc));
    return sizeof(smartAudioU16Frame_t);
}

size_t smartAudioFrameSetOperationMode(uint8_t *buf, size_t bufLen, const vtxDeviceSettings_t *settings)
{
    if (bufLen < SMARTAUDIO_MAX_TX_BUF_SIZE) {
        return 0;
    }
    smartAudioU8Frame_t *frame = (smartAudioU8Frame_t *)buf;
    smartAudioFrameInit(SMARTAUDIO_CMD_SET_MODE, &frame->header, sizeof(frame->payload));
    frame->payload = smartAudioPackOperationMode(settings);
    frame->crc = crc8_dvb_s2_update(0, frame, sizeof(smartAudioU8Frame_t) - sizeof(frame->crc));
    return sizeof(smartAudioU8Frame_t);
}

bool smartAudioParseResponseBuffer(vtxDeviceSettings_t *settings, const uint8_t *buffer)
{
    const smartAudioFrameHeader_t *header = (const smartAudioFrameHeader_t *)buffer;
    const uint8_t fullFrameLength = sizeof(smartAudioFrameHeader_t) + header->length;
    const uint8_t headerPayloadLength = fullFrameLength - 1; // subtract crc byte from length
    const uint8_t *endPtr = buffer + fullFrameLength;

    if (crc8_dvb_s2_update(*endPtr, buffer, headerPayloadLength) || header->startCode != SMARTAUDIO_START_CODE) {
        return false;
    }
    switch (header->command) {
        case SMARTAUDIO_RSP_GET_SETTINGS_V1: {
                const smartAudioSettingsResponseFrame_t *resp = (const smartAudioSettingsResponseFrame_t *)buffer;
                settings->version = 1;
                smartAudioUnpackSettings(settings, resp);
            }
            break;
        case SMARTAUDIO_RSP_GET_SETTINGS_V2: {
                const smartAudioSettingsResponseFrame_t *resp = (const smartAudioSettingsResponseFrame_t *)buffer;
                settings->version = 2;
                smartAudioUnpackSettings(settings, resp);
            }
            break;
        case SMARTAUDIO_RSP_SET_POWER: {
                const smartAudioU16ResponseFrame_t *resp = (const smartAudioU16ResponseFrame_t *)buffer;
                settings->channel = (resp->payload >> 8) & 0xFF;
                const uint8_t power = resp->payload & 0xFF;
                switch (settings->version) {
                    case SMARTAUDIO_VERSION_1:
                        settings->power = smartAudioDacValueToPowerLevel(power);
                        break;
                    case SMARTAUDIO_VERSION_2:
                        settings->power = power;
                        break;
                    default:
                        settings->power = 0;
                        break;
                }
            }
            break;
        case SMARTAUDIO_RSP_SET_CHANNEL: {
                const smartAudioU8ResponseFrame_t *resp = (const smartAudioU8ResponseFrame_t *)buffer;
                settings->channel = resp->payload;
            }
            break;
        case SMARTAUDIO_RSP_SET_FREQUENCY: {
                const smartAudioU16ResponseFrame_t *resp = (const smartAudioU16ResponseFrame_t *)buffer;
                smartAudioUnpackFrequency(settings, resp->payload);
            }
            break;
        case SMARTAUDIO_RSP_SET_MODE: {
                const smartAudioU8ResponseFrame_t *resp = (const smartAudioU8ResponseFrame_t*)buffer;
                smartAudioUnpackOperationMode(settings, resp->payload, false);
            }
            break;
        default:
            return false;
    }
    return true;
}
