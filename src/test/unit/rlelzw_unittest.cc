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

#include <stdint.h>
#include <string.h>

extern "C" {
    #include "common/rle_lzw.h"

    const uint8_t dataBuf[] = "AAAABBCCDDEEEFFFFF";
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

#define DATA_BUF_LEN 128
#define OUTPUT_BUF_LEN 128

TEST(rlelzw, rleEncode)
{
    uint8_t outputBuf[OUTPUT_BUF_LEN];
    rleEncode(dataBuf, sizeof(dataBuf), outputBuf, OUTPUT_BUF_LEN);
    uint8_t *p = outputBuf;
    EXPECT_EQ('A',*p++);
    EXPECT_EQ(4, *p++);
    EXPECT_EQ('B', *p++);
    EXPECT_EQ(2, *p++);
    EXPECT_EQ('C', *p++);
    EXPECT_EQ(2, *p++);
    EXPECT_EQ('D', *p++);
    EXPECT_EQ(2, *p++);
    EXPECT_EQ('E', *p++);
    EXPECT_EQ(3, *p++);
    EXPECT_EQ('F', *p++);
    EXPECT_EQ(5, *p);
}

#define INADEQUATE_BUFFER_SIZE 13

TEST(rlelzw, rleEncodeOverflowProtection) {
    uint8_t outputBuf[INADEQUATE_BUFFER_SIZE];
    size_t testLen = rleEncode(dataBuf, sizeof(dataBuf), outputBuf, sizeof(outputBuf));
    EXPECT_LT(testLen, INADEQUATE_BUFFER_SIZE);
    EXPECT_FALSE(testLen % 2);
}

TEST(rlelzw, rleDecode)
{
    uint8_t outputBuf[OUTPUT_BUF_LEN];
    uint8_t decodeBuf[OUTPUT_BUF_LEN];
    size_t encSize = rleEncode(dataBuf, sizeof(dataBuf), outputBuf, sizeof(outputBuf));
    size_t decSize = rleDecode(outputBuf, encSize, decodeBuf, sizeof(decodeBuf));
    EXPECT_FALSE(memcmp(dataBuf, decodeBuf, decSize));
    EXPECT_EQ(sizeof(dataBuf), decSize);
}

TEST(rlelzw, rleDecodeOverflowProtection)
{
    uint8_t outputBuf[OUTPUT_BUF_LEN];
    uint8_t decodeBuf[INADEQUATE_BUFFER_SIZE];
    size_t encSize = rleEncode(dataBuf, sizeof(dataBuf), outputBuf, sizeof(outputBuf));
    size_t decSize = rleDecode(outputBuf, encSize, decodeBuf, sizeof(decodeBuf));
    EXPECT_FALSE(memcmp(dataBuf, decodeBuf, decSize));
    EXPECT_LT(decSize, sizeof(dataBuf));
}

#define LZW_DICT_SIZE 128
#define LZW_OUTPUT_BUF_SIZE 128

typedef struct lzwDictTuple_s {
    uint16_t symbol1 : 12;
    uint16_t symbol2 : 12;
} __attribute__((packed)) lzwDictTuple_t;

TEST(rlelzw, lzwEncode)
{
    uint8_t testData[] = "jejunojejuno";
    lzwDictEntry_t entries[LZW_DICT_SIZE];
    lzwDict_t dict;
    uint8_t outputBuf[LZW_OUTPUT_BUF_SIZE];
    lzwInit(&dict, entries, LZW_DICT_SIZE);
    size_t encodeSize = lzwEncode(&dict, testData, sizeof(testData), outputBuf, sizeof(outputBuf));
}
