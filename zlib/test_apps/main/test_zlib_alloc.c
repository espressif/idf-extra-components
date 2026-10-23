/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "unity.h"
#include "esp_memory_utils.h"
#include "zlib.h"
#include "deflate.h"
#include "inftrees.h"
#include "inflate.h"

static unsigned custom_alloc_calls;
static unsigned custom_free_calls;

static voidpf custom_zalloc(voidpf opaque, unsigned items, unsigned size)
{
    (void)opaque;
    custom_alloc_calls++;
    return malloc((size_t)items * size);
}

static void custom_zfree(voidpf opaque, voidpf addr)
{
    (void)opaque;
    custom_free_calls++;
    free(addr);
}

TEST_CASE("an application allocator is not overridden", "[zlib][alloc]")
{
    custom_alloc_calls = 0;
    custom_free_calls = 0;

    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = custom_zalloc;
    stream.zfree = custom_zfree;
    TEST_ASSERT_EQUAL_INT(Z_OK, deflateInit(&stream, Z_DEFAULT_COMPRESSION));

    TEST_ASSERT_GREATER_THAN_UINT32(0, custom_alloc_calls);

    TEST_ASSERT_EQUAL_INT(Z_OK, deflateEnd(&stream));
    TEST_ASSERT_EQUAL_UINT32(custom_alloc_calls, custom_free_calls);
}

#if CONFIG_ZLIB_PREFER_PSRAM

static void assert_in_psram(const void *ptr)
{
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_TRUE(esp_ptr_external_ram(ptr));
}

static void assert_in_internal(const void *ptr)
{
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_TRUE(esp_ptr_internal(ptr));
    TEST_ASSERT_FALSE(esp_ptr_external_ram(ptr));
}

/* Deflate's window, hash chains and pending buffer, and inflate's window, are
 * the allocations at or above 16 KiB. The state objects are smaller and stay
 * in internal RAM. */
TEST_CASE("stream buffers are allocated in PSRAM", "[zlib][alloc]")
{
    z_stream deflate_stream;
    memset(&deflate_stream, 0, sizeof(deflate_stream));
    TEST_ASSERT_EQUAL_INT(Z_OK, deflateInit(&deflate_stream, Z_DEFAULT_COMPRESSION));

    deflate_state *dstate = (deflate_state *)deflate_stream.state;
    assert_in_internal(dstate);
    assert_in_psram(dstate->window);
    assert_in_psram(dstate->prev);
    assert_in_psram(dstate->head);
    assert_in_psram(dstate->pending_buf);
    TEST_ASSERT_EQUAL_INT(Z_OK, deflateEnd(&deflate_stream));

    uint8_t raw[32];
    memset(raw, 0x5a, sizeof(raw));
    uint8_t packed[128];
    uLongf packed_len = sizeof(packed);
    TEST_ASSERT_EQUAL_INT(Z_OK, compress(packed, &packed_len, raw, sizeof(raw)));

    z_stream inflate_stream;
    memset(&inflate_stream, 0, sizeof(inflate_stream));
    TEST_ASSERT_EQUAL_INT(Z_OK, inflateInit(&inflate_stream));
    uint8_t out[1];
    inflate_stream.next_in = packed;
    inflate_stream.avail_in = (uInt)packed_len;
    inflate_stream.next_out = out;
    inflate_stream.avail_out = sizeof(out);
    /* Z_FINISH to completion skips creating the window. One output byte with
     * Z_NO_FLUSH forces inflate to allocate it. */
    int ret = inflate(&inflate_stream, Z_NO_FLUSH);
    TEST_ASSERT_TRUE(ret == Z_OK || ret == Z_BUF_ERROR);
    TEST_ASSERT_GREATER_THAN_UINT32(0, inflate_stream.total_out);

    struct inflate_state *istate = (struct inflate_state *)inflate_stream.state;
    assert_in_internal(istate);
    assert_in_psram(istate->window);
    TEST_ASSERT_EQUAL_INT(Z_OK, inflateEnd(&inflate_stream));
}
#endif
