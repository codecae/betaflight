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
#include "drivers/serial.h"
#include "interface/vtx.h"
#include "interface/tramp_protocol.h"
#include "io/serial.h"
#include "vtx/vtx.h"
#include "vtx/vtxTramp.h"
#include "vtx/vtxFrequencyTable.h"

#define TRAMP_POWER_LEVELS  5

static const uint16_t trampPowerLevels[TRAMP_POWER_LEVELS] = { 25, 100, 200, 400, 600 };

static serialPort_t *trampSerialPort = NULL;
static vtxDeviceSettings_t trampSettings;
static bool requestSettingsUpdate = false;
static bool updatePitMode = false;
static bool pitModeEnabled = false;

static bool readyForUpdate(void)
{
    return (trampSerialPort && !requestSettingsUpdate);
}

bool trampParseResponse(void)
{
    uint8_t rxBuffer[TRAMP_FRAME_LENGTH];
    uint8_t *rxBuf = rxBuffer;
    if (!serialRxBytesWaiting(trampSerialPort)) {
        return false;
    }
    while (serialRxBytesWaiting(trampSerialPort)) {
        *rxBuf++ = serialRead(trampSerialPort);
    }
    return trampParseResponseBuffer(&trampSettings, rxBuffer, sizeof(rxBuffer));
}

bool trampGetSettings(void)
{
    if (!trampSerialPort || !requestSettingsUpdate) {
        return false;
    }
    uint8_t frameBuf[TRAMP_FRAME_LENGTH];
    trampFrameGetSettings(frameBuf, sizeof(frameBuf));
    serialWriteBuf(trampSerialPort, frameBuf, sizeof(frameBuf));
    requestSettingsUpdate = false;
    return true;
}

bool trampSetPower(void)
{
    if (!readyForUpdate()) {
        return false;
    }
    const uint8_t configuredPowerLevel = vtxCurrentState->power;
    const uint16_t configuredPower = trampPowerLevels[configuredPowerLevel];
    if (configuredPowerLevel >= TRAMP_POWER_LEVELS || configuredPower == trampSettings.power) {
        return false;
    }
    uint8_t frameBuf[TRAMP_FRAME_LENGTH];
    trampFrameSetPower(frameBuf, sizeof(frameBuf), configuredPower);
    serialWriteBuf(trampSerialPort, frameBuf, sizeof(frameBuf));
    requestSettingsUpdate = true;
    return true;
}

bool trampSetFrequency(void)
{
    if (!readyForUpdate()) {
        return false;
    }
    const uint16_t bandChannelFrequency = vtxBandchan2Freq(vtxCurrentState->band, vtxCurrentState->channel);
    const uint16_t frequency = vtxCurrentState->customFrequencyMode ? vtxCurrentState->frequency : bandChannelFrequency;
    if (frequency < TRAMP_MIN_FREQUENCY || frequency > TRAMP_MAX_FREQUENCY || frequency == trampSettings.frequency) {
        return false;
    }
    uint8_t frameBuf[TRAMP_FRAME_LENGTH];
    trampFrameSetFrequency(frameBuf, sizeof(frameBuf), frequency);
    serialWriteBuf(trampSerialPort, frameBuf, sizeof(frameBuf));
    requestSettingsUpdate = true;
    return true;
}

bool trampProcessPitMode(void)
{
    if (!readyForUpdate() || !updatePitMode) {
        return false;
    }
    uint8_t frameBuf[TRAMP_FRAME_LENGTH];
    const uint8_t activeState = !trampSettings.pitModeEnabled;
    trampFrameSetActiveState(frameBuf, sizeof(frameBuf), activeState);
    serialWriteBuf(trampSerialPort, frameBuf, sizeof(frameBuf));
    updatePitMode = false;
    requestSettingsUpdate = true;
    return true;
}

void trampSetPitmode(const bool pitModeEnable) {
    updatePitMode = (pitModeEnabled != trampSettings.pitModeEnabled);
    if (updatePitMode) {
        trampSettings.pitModeEnabled = pitModeEnable;
    }
}

#define TRAMP_UPDATE_STEP_COUNT 4

static const vtxProcessFn_t trampUpdateStepFn[TRAMP_UPDATE_STEP_COUNT] = {
    trampGetSettings,
    trampSetPower,
    trampSetFrequency,
    trampProcessPitMode
};

static vtxProvider_t trampProvider = {
    .updateSteps = TRAMP_UPDATE_STEP_COUNT,
    .updateStepFn = trampUpdateStepFn,
    .processResponse = trampParseResponse,
    .setPitMode = trampSetPitmode,
};

bool trampProviderInit(void)
{
    if (vtxProviderIsRegistered()) {
        return false;
    }
    serialPortConfig_t *trampPortConfig = findSerialPortConfig(FUNCTION_VTX_TRAMP);
    if (trampPortConfig) {
        portOptions_e portOptions = TRAMP_SERIAL_OPTIONS;
        trampSerialPort = openSerialPort(trampPortConfig->identifier, FUNCTION_VTX_TRAMP, NULL, NULL, TRAMP_BAUD, MODE_RXTX, portOptions);
    }
    if (!trampSerialPort) {
        return false;
    }
    return registerVtxProvider(&trampProvider);
}
