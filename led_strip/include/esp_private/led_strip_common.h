/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PIXEL_FORMAT_3COLORS_MAX LED_PIXEL_FORMAT_RGB

/**
 * @brief Config LED pixel order
 *
 * @param led_pixel_offset Each pixel's offset
 * @param led_pixel_format Input LED strip pixel format
 * @return
 *      - ESP_OK: Config LED pixel order successfully
 *      - ESP_ERR_INVALID_ARG: Config LED pixel order failed because of invalid argument
 *      - ESP_FAIL: Config LED pixel order failed because some other error
 */
esp_err_t led_strip_config_pixel_order(uint8_t *led_pixel_offset, led_pixel_format_t led_pixel_format);

#ifdef __cplusplus
}
#endif
