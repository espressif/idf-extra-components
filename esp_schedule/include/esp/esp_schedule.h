/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** This is the default include file for the esp_schedule component. */
#pragma once

/** Use ESP_SCHEDULE_RETURN_TYPE as the return type for the esp_schedule component. */
#include "esp_err.h"
#define ESP_SCHEDULE_RETURN_TYPE esp_err_t
#define ESP_SCHEDULE_RET_OK ESP_OK
#define ESP_SCHEDULE_RET_FAIL ESP_FAIL
#define ESP_SCHEDULE_RET_NO_MEM ESP_ERR_NO_MEM
#define ESP_SCHEDULE_RET_INVALID_ARG ESP_ERR_INVALID_ARG
#define ESP_SCHEDULE_RET_INVALID_STATE ESP_ERR_INVALID_STATE

#include "esp_schedule_untyped.h"
