/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_mem_impl.h
 * @brief Implementation of the memory allocation glue layer for ESP-IDF.
 */

#pragma once

/** Use esp_heap_caps.h for memory allocation in external RAM. */
#include "esp_heap_caps.h"

#if ((CONFIG_SPIRAM || CONFIG_SPIRAM_SUPPORT) && \
        (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
#define ESP_SCHEDULE_MALLOC(size)        heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define ESP_SCHEDULE_CALLOC(num, size)   heap_caps_calloc_prefer(num, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define ESP_SCHEDULE_REALLOC(ptr, size)  heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#else
#define ESP_SCHEDULE_MALLOC(size)        malloc(size)
#define ESP_SCHEDULE_CALLOC(num, size)   calloc(num, size)
#define ESP_SCHEDULE_REALLOC(ptr, size)  realloc(ptr, size)
#endif

#define ESP_SCHEDULE_FREE(ptr) free(ptr)