/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_mem.h
 * @brief Memory interface for the esp_schedule component
 */

#pragma once

/**
 * The default "glue_mem_impl.h" is provided for ESP-IDF under glue/esp.
 * If you wish to use a different implementation, you can create your own "glue_mem_impl.h" file and not include the default one.
 * - The following memory allocation macros MUST be defined:
 *   - ESP_SCHEDULE_MALLOC(size) : malloc implementation.
 *   - ESP_SCHEDULE_CALLOC(num, size) : calloc implementation.
 *   - ESP_SCHEDULE_REALLOC(ptr, size) : realloc implementation.
 *   - ESP_SCHEDULE_FREE(ptr) : free implementation.
 *
 * This header contributes macros only, so it needs no extern "C" block. The
 * implementation header must not be wrapped in one either: it may pull in
 * system headers of its own, and forcing C linkage on those breaks a C++
 * consumer.
 */
#include "glue_mem_impl.h"

