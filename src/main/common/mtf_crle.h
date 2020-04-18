/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RLE_CHAR_REPEATED_MASK      0x80
#define RLE_DICT_VALUE_MASK         0x7F

size_t mtfCrleEncode(uint8_t *dict, uint8_t *buf, const size_t bufLen);
size_t mtfCrleDecode(uint8_t *dict, const uint8_t *source, const size_t sourceBufLen, uint8_t *dest, const size_t destBufLen);
