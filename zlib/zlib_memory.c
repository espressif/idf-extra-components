/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"

/* Deflate's window, its two hash chains and the pending buffer are 64 KiB each
 * and the inflate window is 32 KiB, while the deflate and inflate states are
 * 6 KiB and 7 KiB. Splitting here keeps the buffers that are walked
 * sequentially in PSRAM and the state that every symbol touches in internal RAM. */
#define ZLIB_PSRAM_THRESHOLD (16 * 1024)

/* MY_ZCALLOC=1 removes zutil.c's definitions, so these become the default
 * allocator that deflateInit2_() and inflateInit2_() install into a stream */
void *zcalloc(void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
    if ((items > 0) && (size > (SIZE_MAX / items))) {
        return NULL;
    }
    size_t bytes = (size_t)items * size;
#if CONFIG_ZLIB_PREFER_PSRAM
    if (bytes >= ZLIB_PSRAM_THRESHOLD) {
        return heap_caps_malloc_prefer(bytes, 2,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return heap_caps_malloc_prefer(bytes, 2,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(bytes);
#endif
}

void zcfree(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}
