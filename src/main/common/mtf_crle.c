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

#include "common/mtf_crle.h"

static size_t cRleEncode(char *buf, const size_t bufLen)
{
    char* cursor = buf;
    for(unsigned int i = 0; i < bufLen; i++) {
        size_t runLength = 1;
        const char c = buf[i];
        for (unsigned int j = i + 1; j < bufLen; j++) {
            if (buf[j] == c) {
                runLength++;
                i++;
            } else {
                break;
            }
        }
        if (runLength > 1) {
            *cursor++ = RLE_CHAR_REPEATED_MASK | c;
            *cursor++ = runLength;
        } else {
            *cursor++ = c;
        }
    }
    return cursor - buf;
}

static size_t cRleDecode(const char *source, char *dest, const size_t sourceBufLen) {
    const char* destStart = dest;
    for (unsigned int i = 0; i < sourceBufLen; i++) {
        const char c = source[i] & RLE_DICT_VALUE_MASK;
        if (source[i] & RLE_CHAR_REPEATED_MASK) {
            uint8_t rep = source[++i];
            while(rep--) {
                *dest++ = c;
            }
        } else {
            *dest++ = c;
        }
    }
    return dest - destStart;
}

static size_t mtfEncode(char *dict, const size_t dictLen, char *buf, const size_t bufLen) {
    size_t encodedLength = 0;
    for (unsigned int i = 0; i < bufLen; i++) {
        for (unsigned int j = 0; j < dictLen; j++) {
            if (buf[i] == dict[j]) {
                const char tmp = *(dict + j);
                memmove(dict + 1, dict, j);
                dict[0] = tmp;
                buf[i] = j;
                encodedLength++;
                break;
            }
        }
    }
    return encodedLength;
}

static size_t mtfDecode(char *dict, char *buf, const size_t bufLen)
{
    size_t decodedLength = 0;
    for (unsigned int i = 0; i < bufLen; i++) {
        const uint8_t idx = buf[i];
        const char tmp = dict[idx];
        memmove(dict + 1, dict, idx);
        dict[0] = tmp;
        buf[i] = tmp;
        decodedLength++;
    }
    buf[bufLen] = '\0';
    return decodedLength;
}

size_t mtfCrleEncode(char *dict, size_t dictLen, char *buf, const size_t bufLen)
{
    const size_t mtfEncodedLength = mtfEncode(dict, dictLen, buf, bufLen);
    return cRleEncode(buf, mtfEncodedLength);
}

size_t mtfCrleDecode(char *dict, const char *source, const size_t sourceBufLen, char *dest)
{
    const size_t rleDecodedLength = cRleDecode(source, dest, sourceBufLen);
    return mtfDecode(dict, dest, rleDecodedLength);
}
