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

#include "platform.h"
#include <stdint.h>
#include <string.h>

#define RLE_CHAR_REPEATED_MASK      0x80
#define RLE_DICT_VALUE_MASK         0x7F

size_t mtfCrleEncode(char *dict, size_t dictLen, char *buf, const size_t bufLen);
size_t mtfCrleDecode(char *dict, const char *source, const size_t sourceBufLen, char *dest);
