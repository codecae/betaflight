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

#pragma once

#include "drivers/serial.h"
#include "interface/vtx.h"

#define SMARTAUDIO_SERIAL_OPTIONS   SERIAL_NOT_INVERTED | SERIAL_BIDIR_NOPULL | SERIAL_STOPBITS_2
#define SMARTAUDIO_DEFAULT_BAUD     4900
#define SMARTAUDIO_MIN_BAUD         4800
#define SMARTAUDIO_MAX_BAUD         4950
#define SMARTAUDIO_MAX_TX_BUF_SIZE  6
#define SMARTAUDIO_MAX_RX_BUF_SIZE  10
#define SMARTAUDIO_POWER_LEVELS     4
#define SMARTAUDIO_MIN_FREQUENCY    5000
#define SMARTAUDIO_MAX_FREQUENCY    5999

enum smartaudioVersion_e {
    SMARTAUDIO_VERSION_UNKNOWN,
    SMARTAUDIO_VERSION_1,
    SMARTAUDIO_VERSION_2,
} smartAudioVersion_t;

struct vtxDeviceSettings_s {
    uint8_t version;
    uint8_t unlocked;
    uint8_t channel;
    uint8_t power;
    uint16_t frequency;
    uint16_t pitModeFrequency;
    bool userFrequencyMode;
    bool pitModeEnabled;
    bool pitModeInRangeActive;
    bool pitModeOutRangeActive;
};

size_t smartAudioFrameGetSettings(uint8_t *buf, size_t bufLen);
size_t smartAudioFrameGetPitModeFrequency(uint8_t *buf, size_t bufLen);
size_t smartAudioFrameSetPower(uint8_t *buf, size_t bufLen, const int version, const uint8_t powerLevel);
size_t smartAudioFrameSetBandChannel(uint8_t *buf, size_t bufLen, const uint8_t band, const uint8_t channel);
size_t smartAudioFrameSetFrequency(uint8_t *buf, size_t bufLen, const uint16_t frequency, const bool pitModeFrequency);
size_t smartAudioFrameSetOperationMode(uint8_t *buf, size_t bufLen, const vtxDeviceSettings_t *settings);
bool smartAudioParseResponseBuffer(vtxDeviceSettings_t *settings, const uint8_t *buffer);
