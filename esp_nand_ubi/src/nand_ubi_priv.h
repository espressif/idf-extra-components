/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stddef.h>
#include <stdlib.h>

#if CONFIG_SPIRAM
#include "esp_heap_caps.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* free() works on both paths: IDF's allocator handles all heap regions. */
static inline void *ubi_alloc(size_t size)
{
#if CONFIG_SPIRAM
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
        return p;
    }
#endif
    return malloc(size);
}

#ifdef __cplusplus
}
#endif
