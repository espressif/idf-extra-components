/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_mem.h
 * @brief Memory interface for the esp_schedule component
 */

#ifndef __GLUE_MEM_H__
#define __GLUE_MEM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The default "glue_mem_impl.h" is provided for ESP-IDF under glue/esp.
 * If you wish to use a different implementation, you can create your own "glue_mem_impl.h" file and not include the default one.
 * - The following memory allocation macros MUST be defined:
 *   - ESP_SCHEDULE_MALLOC(size) : malloc implementation.
 *   - ESP_SCHEDULE_CALLOC(num, size) : calloc implementation.
 *   - ESP_SCHEDULE_REALLOC(ptr, size) : realloc implementation.
 *   - ESP_SCHEDULE_FREE(ptr) : free implementation.
 */
#include "glue_mem_impl.h"

#ifdef __cplusplus
}
#endif

#endif /* __GLUE_MEM_H__ */