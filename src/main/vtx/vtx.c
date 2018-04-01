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

#include <stdbool.h>
#include <string.h>

#include "platform.h"
#include "common/time.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"
#include "interface/vtx.h"
#include "vtx/vtx.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"

#define VTX_DEFAULT_CUSTOM_FREQ_MODE false
#define VTX_DEFAULT_BAND 4
#define VTX_DEFAULT_CHANNEL 1
#define VTX_DEFAULT_POWER 0
#define VTX_DEFAULT_FREQUENCY 5740
#define VTX_DEFAULT_PITMODE_FREQUENCY 5740

PG_REGISTER_WITH_RESET_TEMPLATE(vtxConfig_t, vtxConfig, PG_VTX_CONFIG, 0);

PG_RESET_TEMPLATE(vtxConfig_t, vtxConfig,
    .customFrequencyMode = VTX_DEFAULT_CUSTOM_FREQ_MODE,
    .band = VTX_DEFAULT_BAND,
    .channel = VTX_DEFAULT_CHANNEL,
    .power = VTX_DEFAULT_POWER,
    .frequency = VTX_DEFAULT_FREQUENCY,
    .pitModeFrequency = VTX_DEFAULT_PITMODE_FREQUENCY,
);

static vtxProvider_t *vtxProvider = NULL;
static vtxConfig_t currentState;
const vtxConfig_t *vtxCurrentState = &currentState;

static void vtxUpdateCurrentState(void)
{
    memcpy(&currentState, vtxConfig(), sizeof(vtxConfig_t));

    if (IS_RC_MODE_ACTIVE(BOXVTXPITMODE) && isModeActivationConditionPresent(BOXVTXPITMODE) && currentState.pitModeFrequency) {
        currentState.customFrequencyMode = true;
        currentState.frequency = currentState.pitModeFrequency;
        currentState.power = VTX_DEFAULT_POWER;
    }

    if (!ARMING_FLAG(ARMED) && IS_RC_MODE_ACTIVE(BOXVTXLOWPOWER) && isModeActivationConditionPresent(BOXVTXPITMODE)) {
        currentState.power = VTX_DEFAULT_POWER;
    }
}

bool vtxProviderIsRegistered()
{
    return (bool) vtxProvider;
}

bool registerVtxProvider(vtxProvider_t *provider)
{
    if (vtxProviderIsRegistered()) {
        return false;
    }
    vtxProvider = provider;
    memcpy(&currentState, vtxConfig(), sizeof(currentState));
    return true;
}

void vtxSetPitMode(const bool pitModeEnabled)
{
    if (vtxProviderIsRegistered() && vtxProvider->setPitMode) {
        vtxProvider->setPitMode(pitModeEnabled);
    }
}

void vtxProviderUpdate(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);
    if (!vtxProviderIsRegistered()) {
        return;
    }
    vtxProvider->processResponse();
    vtxUpdateCurrentState();
    for (int i = 0; i < vtxProvider->updateSteps; ++i) {
        if (vtxProvider->updateStepFn[i]()) {
            break;
        }
    }
}
