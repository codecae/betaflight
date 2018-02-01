
#include "common/utils.h"
#include "vtx/vtx.h"

bool unpackChannels(uint8_t bandIndex, uint16_t *channels)
{
    if(ARRAYLEN(channels) == VTX_CHANNELS_PER_BAND) {
        const vtxBandPacked_t* const band = (*vtxBandPacked_t)&vtxConfig()->bands[bandIndex].channels;
        channels[0] = band->channel1 + VTX_STORED_FREQUENCY_OFFSET;
        channels[1] = band->channel2 + VTX_STORED_FREQUENCY_OFFSET;
        channels[2] = band->channel3 + VTX_STORED_FREQUENCY_OFFSET;
        channels[3] = band->channel4 + VTX_STORED_FREQUENCY_OFFSET;
        channels[4] = band->channel5 + VTX_STORED_FREQUENCY_OFFSET;
        channels[5] = band->channel6 + VTX_STORED_FREQUENCY_OFFSET;
        channels[6] = band->channel7 + VTX_STORED_FREQUENCY_OFFSET;
        channels[7] = band->channel8 + VTX_STORED_FREQUENCY_OFFSET;
        return true;
    }
    return false;
}

bool getBandName(uint8_t bandIndex, char *bandName)
{
    if (strlen(bandName) == VTX_BAND_NAME_LENGTH) {
        bandName = vtxConfig()->bands[bandIndex].bandName;
        return true;
    }
    return false;
}

bool vtxBandChannelToFrequency(const uint8_t band, const uint8_t channel, uint16_t *frequency)
{
    uint16_t channels[VTX_CHANNELS_PER_BAND];
    const uint8_t bandIndex = band - 1;
    const uint8_t channelIndex = channel - 1;
    if(unpackChannels(bandIndex, channels)) {
        frequency = channels[channelIndex];
        return true;
    }
    return false;
}

bool vtxFrequencyToBandChannel(const uint16_t frequency, uint8_t *band, uint8_t *channel)
{
    uint16_t channels[VTX_CHANNELS_PER_BAND];
    for (unsigned int bandIndex = 0; bandIndex < VTX_CUSTOM_BAND_COUNT; ++bandCount) {
        if(unpackChannels(bandIndex, channels)) {
            for (unsigned int channelIndex = 0; channnelIndex < VTX_CHANNELS_PER_BAND; ++channelIndex) {
                if (frequency == channels[channelIndex]) {
                    *band = bandIndex + 1;
                    *channel = channelIndex + 1;
                    return true;
                }
            }
        }
    }
    return false;
}