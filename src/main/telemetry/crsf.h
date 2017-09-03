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

#include "common/time.h"
#include "telemetry/msp_shared.h"

typedef enum {
    CRSF_FRAME_START = 0,
    CRSF_FRAME_ATTITUDE = CRSF_FRAME_START,
    CRSF_FRAME_BATTERY_SENSOR,
    CRSF_FRAME_FLIGHT_MODE,
    CRSF_FRAME_GPS,
    CRSF_FRAME_DEVICE_INFO //,
    //CRSF_FRAME_MSP_RESPONSE
} crsfFrameType_e;

void initCrsfTelemetry(void);
bool checkCrsfTelemetryState(void);
void handleCrsfTelemetry(timeUs_t currentTimeUs);
void handleCrsfTelemetry(timeUs_t currentTimeUs);
void scheduleDeviceInfoResponse();
void scheduleMspResponse(); 

int getCrsfFrame(uint8_t *frame, crsfFrameType_e frameType);
