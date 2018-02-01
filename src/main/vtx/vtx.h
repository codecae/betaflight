#pragma once

#include "platform.h"
#include "pg/pg.h"

#define VTX_BANDS 6
#define VTX_CHANNELS_PER_BAND 8
#define VTX_BAND_NAME_LENGTH 8
#define VTX_STORED_FREQUENCY_OFFSET 5000

typedef enum {
    VTX_PROTOCOL_NONE,
    VTX_PROTOCOL_SMARTAUDIO,
    VTX_PROTOCOL_TRAMP,
    VTX_PROTOCOL_RTC6705,
    VTX_PROTOCOL_COUNT = VTX_PROTOCOL_RTC6705,
} vtxSupportedProtocol_t;

typedef struct vtxChannelsPacked_s {
    unsigned int channel1 : 10;
    unsigned int channel2 : 10;
    unsigned int channel3 : 10;
    unsigned int channel4 : 10;
    unsigned int channel5 : 10;
    unsigned int channel6 : 10;
    unsigned int channel7 : 10;
    unsigned int channel8 : 10;
} vtxChannelsPacked_t __attribute__((packed));

typedef struct vtxBand_s {
    char bandName[VTX_BAND_NAME_LENGTH];
    vtxChannelsPacked_t channels;
} vtxBand_t;

typedef struct vtxConfig_s {
    vtxBandPacked_t bands[VTX_BANDS];
    uint16_t freq;
    uint16_t pitModeFreq;
    uint8_t lowPowerDisarm;
} vtxConfig_t;

typedef struct vtxDevice_s {
    vtxSupportedProtocol_t protocol;
    uint16_t (*vtxGetFrequencyFnPtr)(void);
    uint16_t (*vtxGetPowerFnPtr)(void);
    bool (*vtxSetFrequencyFnPtr)(uint16_t frequency);
    bool (*vtxSetPowerFnPtr)(uint16_t mwPower);
    bool (*vtxTaskFnPtr)(void);
} vtxDevice_t;

PG_DECLARE(vtxConfig_t, vtxConfig);

bool getBandName(uint8_t bandIndex, char *bandName)
bool vtxBandChannelToFrequency(const uint8_t band, const uint8_t channel, uint16_t *frequency);
bool vtxFrequencyToBandChannel(const uint16_t frequency, uint8_t *band, uint8_t *channel);