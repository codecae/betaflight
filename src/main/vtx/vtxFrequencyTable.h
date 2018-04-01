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

#define VTX_BAND_COUNT 5
#define VTX_CHAN_COUNT 8

#include "platform.h"

extern const uint16_t vtxfrequencyTable[VTX_BAND_COUNT][VTX_CHAN_COUNT];
extern const char * const vtxBandNames[];
extern const char * const vtxChannelNames[];
extern const char vtxBandLetter[];

bool vtxFreq2Bandchan(uint16_t freq, uint8_t *pBand, uint8_t *pChannel);
uint16_t vtxBandchan2Freq(uint8_t band, uint8_t channel);
