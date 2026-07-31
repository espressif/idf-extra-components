/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** This is the default include file for the esp_schedule component. */
#pragma once

/* esp_schedule_untyped.h gates public struct members on
 * CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT, so sdkconfig.h must be visible before it
 * is included. Without it the macro is silently 0 and the application disagrees
 * with the library about sizeof(esp_schedule_config_t) - no diagnostic, just a
 * write past the caller's struct. Relying on esp_err.h to pull it in
 * incidentally would leave that guarantee undocumented. */
#include "sdkconfig.h"

/** Use ESP_SCHEDULE_RETURN_TYPE as the return type for the esp_schedule component. */
#include "esp_err.h"
#define ESP_SCHEDULE_RETURN_TYPE esp_err_t
#define ESP_SCHEDULE_RET_OK ESP_OK
#define ESP_SCHEDULE_RET_FAIL ESP_FAIL
#define ESP_SCHEDULE_RET_NO_MEM ESP_ERR_NO_MEM
#define ESP_SCHEDULE_RET_INVALID_ARG ESP_ERR_INVALID_ARG
#define ESP_SCHEDULE_RET_INVALID_STATE ESP_ERR_INVALID_STATE

#include "esp_schedule_untyped.h"
