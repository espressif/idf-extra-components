/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"

#if CONFIG_LZ4_USE_PSRAM

/*
 * Prefer PSRAM for LZ4 workspaces, but fall back to internal 8-bit RAM when
 * PSRAM is unavailable or cannot satisfy the allocation. heap_caps_free() can
 * release memory from either heap.
 */
void *LZ4_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_8BIT);
}

void *LZ4_calloc(size_t count, size_t size)
{
    return heap_caps_calloc_prefer(count, size, 2,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_8BIT);
}

void LZ4_free(void *ptr)
{
    heap_caps_free(ptr);
}

#endif /* CONFIG_LZ4_USE_PSRAM */
