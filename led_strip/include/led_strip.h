/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_rmt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set RGB for a specific pixel
 *
 * @param strip: LED strip
 * @param index: index of pixel to set
 * @param red: red part of color
 * @param green: green part of color
 * @param blue: blue part of color
 *
 * @return
 *      - ESP_OK: Set RGB for a specific pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set RGB for a specific pixel failed because of invalid parameters
 *      - ESP_FAIL: Set RGB for a specific pixel failed because other error occurred
 */
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Refresh memory colors to LEDs
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Refresh successfully
 *      - ESP_FAIL: Refresh failed because some other error occurred
 *
 * @note:
 *      After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to strip.
 */
esp_err_t led_strip_refresh(led_strip_handle_t strip);

/**
 * @brief Clear LED strip (turn off all LEDs)
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Clear LEDs successfully
 *      - ESP_FAIL: Clear LEDs failed because some other error occurred
 */
esp_err_t led_strip_clear(led_strip_handle_t strip);

/**
 * @brief Free LED strip resources
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Free resources successfully
 *      - ESP_FAIL: Free resources failed because error occurred
 */
esp_err_t led_strip_del(led_strip_handle_t strip);

#ifdef __cplusplus
}
#endif
