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

#include "platform.h"
#include "drivers/serial.h"
#include "interface/vtx.h"
#include "interface/smartaudio_protocol.h"
#include "io/serial.h"
#include "vtx/vtx.h"
#include "vtx/vtxSmartAudio.h"
#include "vtx/vtxFrequencyTable.h"

static serialPort_t *smartAudioSerialPort = NULL;
static vtxDeviceSettings_t smartAudioSettings;
static bool requestSettingsUpdate = false;
static bool updatePitMode = false;

static bool readyForUpdate(void)
{
    return (smartAudioSerialPort && !requestSettingsUpdate);
}

static void sendTxFrame(uint8_t *buf, size_t bufLength)
{
    switch (smartAudioSerialPort->identifier) {
        case SERIAL_PORT_SOFTSERIAL1:
        case SERIAL_PORT_SOFTSERIAL2:
            break;
        default:
            serialWrite(smartAudioSerialPort, 0x00); // Generate 1st start bit
            break;
    }
    serialWriteBuf(smartAudioSerialPort, buf, bufLength);
}

static bool smartAudioSetFrequencyEx(const uint16_t frequency, const bool pitmodeFrequency)
{
    if (!readyForUpdate() || frequency < SMARTAUDIO_MIN_FREQUENCY || frequency > SMARTAUDIO_MAX_FREQUENCY) {
        return false;
    }
    uint8_t frameBuf[SMARTAUDIO_MAX_TX_BUF_SIZE];
    size_t frameLength = smartAudioFrameSetFrequency(frameBuf, sizeof(frameBuf), frequency, pitmodeFrequency);
    sendTxFrame(frameBuf, frameLength);
    requestSettingsUpdate = true;
    return true;
}

bool smartAudioParseResponse(void)
{
    uint8_t rxBuffer[SMARTAUDIO_MAX_RX_BUF_SIZE];
    uint8_t *rxBuf = rxBuffer;
    if (!serialRxBytesWaiting(smartAudioSerialPort)) {
        return false;
    }
    while (serialRxBytesWaiting(smartAudioSerialPort)) {
        *rxBuf++ = serialRead(smartAudioSerialPort);
    }
    return smartAudioParseResponseBuffer(&smartAudioSettings, rxBuffer);
}

bool smartAudioGetSettings(void)
{
    if (!readyForUpdate()) {
        return false;
    }
    uint8_t frameBuf[SMARTAUDIO_MAX_TX_BUF_SIZE];
    const size_t frameLength = smartAudioFrameGetSettings(frameBuf, sizeof(frameBuf));
    sendTxFrame(frameBuf, frameLength);
    requestSettingsUpdate = false;
    return true;
}

bool smartAudioGetPitmodeFrequency(void)
{
    if (!readyForUpdate()) {
        return false;
    }
    uint8_t frameBuf[SMARTAUDIO_MAX_TX_BUF_SIZE];
    const size_t frameLength = smartAudioFrameGetPitModeFrequency(frameBuf, sizeof(frameBuf));
    sendTxFrame(frameBuf, frameLength);
    requestSettingsUpdate = true;
    return true;
}

bool smartAudioSetPower(void)
{
    if (!readyForUpdate()) {
        return false;
    }
    uint8_t frameBuf[SMARTAUDIO_MAX_TX_BUF_SIZE];
    const size_t frameLength = smartAudioFrameSetPower(frameBuf, sizeof(frameBuf), smartAudioSettings.version, vtxCurrentState->power);
    sendTxFrame(frameBuf, frameLength);
    return true;
}

bool smartAudioSetFrequency(void)
{
    const uint16_t bandChannelFrequency = vtxBandchan2Freq(vtxCurrentState->band, vtxCurrentState->channel);
    const uint16_t frequency = vtxCurrentState->customFrequencyMode ? vtxCurrentState->frequency : bandChannelFrequency;
    return smartAudioSetFrequencyEx(frequency, false);
}

bool smartAudioSetPitmodeFrequency(void)
{
    return smartAudioSetFrequencyEx(vtxCurrentState->pitModeFrequency, true);
}

bool smartAudioProcessPitmode(void)
{
    if (!readyForUpdate() || !updatePitMode || !smartAudioSettings.pitModeEnabled) {
        return false;
    }
    uint8_t frameBuf[SMARTAUDIO_MAX_TX_BUF_SIZE];
    size_t frameLength = smartAudioFrameSetOperationMode(frameBuf, sizeof(frameBuf), &smartAudioSettings);
    sendTxFrame(frameBuf, frameLength);
    updatePitMode = false;
    return true;
}

void smartAudioSetPitMode(const bool pitModeEnable)
{
    if (pitModeEnable) {
        return;
    }
    updatePitMode = pitModeEnable != smartAudioSettings.pitModeEnabled;
    if (updatePitMode) {
        smartAudioSettings.pitModeEnabled = pitModeEnable;
    }
}

#define SMARTAUDIO_UPDATE_STEP_COUNT 6

static const vtxProcessFn_t smartAudioUpdateStepFn[SMARTAUDIO_UPDATE_STEP_COUNT] = {
    smartAudioGetSettings,
    smartAudioGetPitmodeFrequency,
    smartAudioSetPower,
    smartAudioSetFrequency,
    smartAudioSetPitmodeFrequency,
    smartAudioProcessPitmode
};

static vtxProvider_t smartAudioProvider = {
    .updateSteps = SMARTAUDIO_UPDATE_STEP_COUNT,
    .updateStepFn = smartAudioUpdateStepFn,
    .processResponse = smartAudioParseResponse,
    .setPitMode = smartAudioSetPitMode
};

bool smartAudioProviderInit(void)
{
    if (vtxProviderIsRegistered()) {
        return false;
    }
    serialPortConfig_t *smartAudioPortConfig = findSerialPortConfig(FUNCTION_VTX_SMARTAUDIO);
    if (smartAudioPortConfig) {
        portOptions_e portOptions = SMARTAUDIO_SERIAL_OPTIONS;
        smartAudioSerialPort = openSerialPort(smartAudioPortConfig->identifier, FUNCTION_VTX_SMARTAUDIO, NULL, NULL, SMARTAUDIO_DEFAULT_BAUD, MODE_RXTX, portOptions);
    }
    if (!smartAudioSerialPort) {
        return false;
    }
    smartAudioSettings.version = 0;
    return registerVtxProvider(&smartAudioProvider);
}
