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

#include "stdbool.h"
#include "string.h"
#include "common/rle_lzw.h"

size_t rleEncode(const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen)
{
    const uint8_t *d = data;
    uint8_t *b = buf;
    uint8_t currentByte;
    uint8_t lastByte;
    size_t length = 1;

    for (unsigned int i=0; i<dataLen; ++i) {
        currentByte = *d++;
        if (i) {
            if (currentByte == lastByte) {
                ++length;
                continue;
            }
            if ((bufLen - (b-buf)) <= RLE_ENCODED_TUPLE_SIZE) {
                break;
            }
            *b++ = length;
        }
        *b++ = currentByte;
        length = 1;
        lastByte = currentByte;
    }
    *b++ = length;
    return (b-buf);
}

size_t rleDecode(const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen)
{
    const uint8_t *d = data;
    uint8_t *b = buf;
    uint8_t currentByte;
    size_t length;

    for (unsigned int i=0; i<dataLen/2; i++) {
        currentByte = *d++;
        length = *d++;
        for (unsigned int j=0; j<length; j++) {
            if ((bufLen - (b-buf)) > 0) {
                *b++ = currentByte;
            }
        }
    }
    return (b-buf);
}

void lzwInit(lzwDict_t *dict, lzwDictEntry_t *entries, size_t maxEntries)
{
    dict->entries = entries;
    dict->size = 0;
    dict->maxSize = maxEntries;
}

static uint16_t lzwDictLookup(lzwDict_t *dict, const uint16_t symbol1, const uint16_t symbol2)
{
    lzwDictEntry_t *e;
    for (unsigned int i=0; i<dict->size; i++) {
        e = &dict->entries[i];
        if (e->symbol1 == symbol1 && e->symbol2 == symbol2) {
            return e->code | 0x100;
        }
    }
    return symbol1;
}

static void lzwDictUpdate(lzwDict_t *dict, const uint16_t symbol1, const uint16_t symbol2)
{
    lzwDictEntry_t *e = &dict->entries[dict->size];
    e->symbol1 = symbol1;
    e->symbol2 = symbol2;
    e[dict->size].code = ++dict->size;
}

size_t lzwEncode(lzwDict_t *dict, const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen)
{
    (void)bufLen;

    const uint8_t *d = data;
    uint16_t current = *d++;
    uint16_t next = *d++;
    uint8_t *b = buf;
    bool firstByte = true;

    while (dataLen >= (size_t)(d - data)) {      
        uint16_t code = lzwDictLookup(dict, current, next);
        if (code != current) {
            current = code;
            next = *d++;
            continue;
        }
        lzwDictUpdate(dict, current, next);
        if (firstByte) {
            *b++ = (current & 0xFF);
            *b = (current >> 8);
        } else {
            *b++ |= (current << 4);
            *b++ = (current >> 4);
        }
        firstByte = !firstByte;
        current = next;
        next = *d++;
    }

    return (b-buf);
}

size_t lzwDecode(lzwDict_t *dict, const uint8_t *data, const size_t dataLen, uint8_t *buf, const size_t bufLen)
{
    (void)bufLen;
    
    const uint8_t *d = data;
    uint8_t *b = buf;
    size_t encodedLength = (dataLen * 8) / 12;

    while (encodedLength--) {
        
    }
}