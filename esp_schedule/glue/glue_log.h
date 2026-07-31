/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_log.h
 * @brief Glue layer for logging.
 */

#pragma once

/**
 * The default "glue_log_impl.h" is provided for ESP-IDF under glue/esp.
 * If you wish to use a different implementation, you can create your own "glue_log_impl.h" file and not include the default one.
 * - The following logging function macros MUST be defined:
 *   - ESP_SCHEDULE_LOGE(tag, fmt, ...) : Log an error message.
 *   - ESP_SCHEDULE_LOGW(tag, fmt, ...) : Log a warning message.
 *   - ESP_SCHEDULE_LOGI(tag, fmt, ...) : Log an info message.
 *   - ESP_SCHEDULE_LOGD(tag, fmt, ...) : Log a debug message.
 *   - ESP_SCHEDULE_LOGV(tag, fmt, ...) : Log a verbose message.
 *
 * This header contributes macros only, so it needs no extern "C" block. The
 * implementation header must not be wrapped in one either: it may pull in
 * system headers of its own, and forcing C linkage on those breaks a C++
 * consumer.
 */
#include "glue_log_impl.h"

