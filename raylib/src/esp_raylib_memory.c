/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Memory backing for raylib and its rlsw software renderer.
 *
 * With PSRAM, large blocks (framebuffers, textures) prefer PSRAM and small
 * bookkeeping blocks prefer internal RAM, which is faster and DMA-capable;
 * each falls back to the other region when the preferred heap is exhausted.
 */

#include <stdint.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_raylib_memory.h"

/* Framebuffers and texture atlases are the blocks worth a PSRAM trip; below
 * this line the renderer touches the memory constantly and internal RAM wins on latency. */
#define RAYLIB_PSRAM_THRESHOLD (32 * 1024)

void *esp_raylib_malloc(size_t size)
{
#if CONFIG_RAYLIB_USE_PSRAM
    if (size >= RAYLIB_PSRAM_THRESHOLD) {
        return heap_caps_malloc_prefer(size, 2,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return heap_caps_malloc_prefer(size, 2,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

void *esp_raylib_calloc(size_t count, size_t size)
{
#if CONFIG_RAYLIB_USE_PSRAM
    if ((count != 0) && (size > (SIZE_MAX / count))) {
        return NULL;
    }

    if (count * size >= RAYLIB_PSRAM_THRESHOLD) {
        return heap_caps_calloc_prefer(count, size, 2,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return heap_caps_calloc_prefer(count, size, 2,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return calloc(count, size);
#endif
}

void *esp_raylib_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return esp_raylib_malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

#if CONFIG_RAYLIB_USE_PSRAM
    if (size >= RAYLIB_PSRAM_THRESHOLD) {
        return heap_caps_realloc_prefer(ptr, size, 2,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return heap_caps_realloc_prefer(ptr, size, 2,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return realloc(ptr, size);
#endif
}

void esp_raylib_free(void *ptr)
{
    free(ptr);
}
