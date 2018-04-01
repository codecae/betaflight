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

#define TRAMP_SERIAL_OPTIONS    SERIAL_NOT_INVERTED | SERIAL_BIDIR
#define TRAMP_BAUD              9600
#define TRAMP_FRAME_LENGTH      16
#define TRAMP_MIN_FREQUENCY     5000
#define TRAMP_MAX_FREQUENCY     5999

struct vtxDeviceSettings_s {
    uint16_t frequency;
    uint16_t power;
    uint8_t raceModeEnabled;
    uint8_t pitModeEnabled;
};

bool trampFrameGetSettings(uint8_t *buf, size_t bufLen);
bool trampFrameSetFrequency(uint8_t *buf, size_t bufLen, const uint16_t frequency);
bool trampFrameSetPower(uint8_t *buf, size_t bufLen, const uint16_t power);
bool trampFrameSetActiveState(uint8_t *buf, size_t bufLen, const bool active);
bool trampParseResponseBuffer(vtxDeviceSettings_t *settings, const uint8_t *buf, size_t bufLen);
