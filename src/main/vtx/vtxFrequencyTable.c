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
#include "vtx/vtxFrequencyTable.h"

const uint16_t vtxFrequencyTable[VTX_BAND_COUNT][VTX_CHAN_COUNT] =
{
    { 5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725 }, // Boscam A
    { 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866 }, // Boscam B
    { 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945 }, // Boscam E
    { 5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880 }, // FatShark
    { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 }, // RaceBand
};

const char * const vtxBandNames[] = {
    "--------",
    "BOSCAM A",
    "BOSCAM B",
    "BOSCAM E",
    "FATSHARK",
    "RACEBAND",
};

const char vtxBandLetter[] = "-ABEFR";

const char * const vtxChannelNames[] = {
    "-", "1", "2", "3", "4", "5", "6", "7", "8",
};

bool vtxFreq2Bandchan(uint16_t freq, uint8_t *pBand, uint8_t *pChannel)
{
    for (int band = VTX_BAND_COUNT - 1 ; band >= 0 ; band--) {
        for (int channel = 0 ; channel < VTX_CHAN_COUNT ; channel++) {
            if (vtxFrequencyTable[band][channel] == freq) {
                *pBand = band + 1;
                *pChannel = channel + 1;
                return true;
            }
        }
    }
    *pBand = 0;
    *pChannel = 0;
    return false;
}

uint16_t vtxBandchan2Freq(uint8_t band, uint8_t channel)
{
    if (band > 0 && band <= VTX_BAND_COUNT && channel > 0 && channel <= VTX_CHAN_COUNT) {
        return vtxFrequencyTable[band - 1][channel - 1];
    }
    return 0;
}
