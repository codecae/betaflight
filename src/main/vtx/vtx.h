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

#include <stdbool.h>

#include "common/time.h"
#include "interface/vtx.h"
#include "pg/pg.h"

typedef struct vtxConfig_s {
    uint8_t customFrequencyMode;
    uint8_t band;
    uint8_t channel;
    uint8_t power;
    uint16_t frequency;
    uint16_t pitModeFrequency;
} vtxConfig_t;

PG_DECLARE(vtxConfig_t, vtxConfig);

typedef bool (*vtxProcessFn_t)(void);

typedef struct vtxProvider_s {
    const uint8_t updateSteps;
    const vtxProcessFn_t *updateStepFn;
    const vtxProcessFn_t processResponse;
    void (*setPitMode)(const bool);
} vtxProvider_t;

bool vtxProviderIsRegistered();
bool registerVtxProvider(vtxProvider_t *provider);
void vtxSetPitMode(const bool pitModeEnabled);
void vtxProviderUpdate(timeUs_t currentTimeUs);

extern const vtxConfig_t *vtxCurrentState;