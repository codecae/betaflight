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

#include "platform.h"

#define RLE_ENCODED_TUPLE_SIZE 2

typedef struct lzwDictEntry_s {
    uint16_t symbol1 : 12;
    uint16_t symbol2 : 12;
    uint8_t code;
} __attribute__((packed)) lzwDictEntry_t;

typedef struct lzwDict_s {
    lzwDictEntry_t *entries;
    size_t size;
    size_t maxSize;
} lzwDict_t;

size_t rleEncode(const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen);
size_t rleDecode(const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen);
void lzwInit(lzwDict_t *dict, lzwDictEntry_t *entries, size_t maxEntries);
size_t lzwEncode(lzwDict_t *dict, const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen);
