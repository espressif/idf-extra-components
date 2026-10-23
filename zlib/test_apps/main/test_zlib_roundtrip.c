/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "zlib.h"

#define SRC_SIZE (32 * 1024)

/* Reproducible payload for every round trip. The recurrence is the
 * Numerical Recipes 32-bit LCG, so the bytes stay fixed across libc
 * implementations. An LCG's low bits have a short period, so the stored
 * byte is the high byte mixed with the index and with the index shifted
 * by 3. That mix fills the deflate window with data its hash chain has
 * to keep, which is what the round-trip cases check. */
static void fill_deterministic(uint8_t *buf, size_t len)
{
    uint32_t state = 0xA5A5A5A5u;

    for (size_t i = 0; i < len; i++) {
        state = (state * 1664525u) + 1013904223u;
        buf[i] = (uint8_t)((state >> 24) ^ (uint32_t)i ^ ((uint32_t)i << 3));
    }
}

TEST_CASE("compress2 and uncompress round trip", "[zlib][compress]")
{
    uint8_t *src = malloc(SRC_SIZE);
    TEST_ASSERT_NOT_NULL(src);
    fill_deterministic(src, SRC_SIZE);

    uLongf capacity = compressBound(SRC_SIZE);
    uint8_t *packed = malloc(capacity);
    TEST_ASSERT_NOT_NULL(packed);

    TEST_ASSERT_EQUAL_INT(Z_OK, compress2(packed, &capacity, src, SRC_SIZE,
                                          Z_DEFAULT_COMPRESSION));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32((uint32_t)compressBound(SRC_SIZE),
                                     (uint32_t)capacity);

    uint8_t *restored = malloc(SRC_SIZE);
    TEST_ASSERT_NOT_NULL(restored);
    uLongf restored_len = SRC_SIZE;
    TEST_ASSERT_EQUAL_INT(Z_OK, uncompress(restored, &restored_len,
                                           packed, capacity));

    TEST_ASSERT_EQUAL_UINT32(SRC_SIZE, (uint32_t)restored_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, restored, SRC_SIZE);

    free(restored);
    free(packed);
    free(src);
}

TEST_CASE("deflate and inflate stream round trip", "[zlib][compress]")
{
    uint8_t *src = malloc(SRC_SIZE);
    TEST_ASSERT_NOT_NULL(src);
    fill_deterministic(src, SRC_SIZE);

    z_stream deflate_stream;
    memset(&deflate_stream, 0, sizeof(deflate_stream));
    TEST_ASSERT_EQUAL_INT(Z_OK, deflateInit(&deflate_stream, Z_DEFAULT_COMPRESSION));

    uLongf capacity = deflateBound(&deflate_stream, SRC_SIZE);
    uint8_t *packed = malloc(capacity);
    TEST_ASSERT_NOT_NULL(packed);

    deflate_stream.next_in = src;
    deflate_stream.avail_in = SRC_SIZE;
    deflate_stream.next_out = packed;
    deflate_stream.avail_out = (uInt)capacity;
    TEST_ASSERT_EQUAL_INT(Z_STREAM_END, deflate(&deflate_stream, Z_FINISH));
    const uLong packed_len = capacity - deflate_stream.avail_out;
    TEST_ASSERT_EQUAL_INT(Z_OK, deflateEnd(&deflate_stream));

    z_stream inflate_stream;
    memset(&inflate_stream, 0, sizeof(inflate_stream));
    TEST_ASSERT_EQUAL_INT(Z_OK, inflateInit(&inflate_stream));

    uint8_t *restored = malloc(SRC_SIZE);
    TEST_ASSERT_NOT_NULL(restored);
    inflate_stream.next_in = packed;
    inflate_stream.avail_in = (uInt)packed_len;
    inflate_stream.next_out = restored;
    inflate_stream.avail_out = SRC_SIZE;
    TEST_ASSERT_EQUAL_INT(Z_STREAM_END, inflate(&inflate_stream, Z_FINISH));
    TEST_ASSERT_EQUAL_UINT32(0, inflate_stream.avail_out);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, restored, SRC_SIZE);

    TEST_ASSERT_EQUAL_INT(Z_OK, inflateEnd(&inflate_stream));
    free(restored);
    free(packed);
    free(src);
}
