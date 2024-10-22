/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_rmt.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#include "led_strip_spi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Help macro to set pixel RGB color order
 *        The default order of the three-color LED strips is GRB. If you have a different order, you can use the macro to set `pixel_order` in led_strip_config_t.
 *        The positions are counted from the least significant bit (LSB).
 * @param R The position of the red channel in the color order.
 * @param G The position of the green channel in the color order.
 * @param B The position of the blue channel in the color order.
 * @note The order starts from 0. And the user needs to make sure that all the numbers appear exactly once and are all less than the number of colors per pixel.
 */
#define LED_STRIP_SET_RGB_ORDER(R, G, B) (R << 0 | G << 2 | B << 4)

/**
 * @brief Help macro to set pixel RGBW color order
 *        The default order of the four-color LED strips is GRBW. If you have a different order, you can use the macro to set `pixel_order` in led_strip_config_t.
 *        The positions are counted from the least significant bit (LSB).
 * @param R The position of the red channel in the color order.
 * @param G The position of the green channel in the color order.
 * @param B The position of the blue channel in the color order.
 * @param W The position of the white channel in the color order.
 * @note The order starts from 0. And the user needs to make sure that all the numbers appear exactly once and are all less than the number of colors per pixel.
 */
#define LED_STRIP_SET_RGBW_ORDER(R, G, B, W) (R << 0 | G << 2 | B << 4 | W << 6)

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
 * @brief Set RGBW for a specific pixel
 *
 * @note Only call this function if your led strip does have the white component (e.g. SK6812-RGBW)
 * @note Also see `led_strip_set_pixel` if you only want to specify the RGB part of the color and bypass the white component
 *
 * @param strip: LED strip
 * @param index: index of pixel to set
 * @param red: red part of color
 * @param green: green part of color
 * @param blue: blue part of color
 * @param white: separate white component
 *
 * @return
 *      - ESP_OK: Set RGBW color for a specific pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set RGBW color for a specific pixel failed because of an invalid argument
 *      - ESP_FAIL: Set RGBW color for a specific pixel failed because other error occurred
 */
esp_err_t led_strip_set_pixel_rgbw(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white);

/**
 * @brief Set HSV for a specific pixel
 *
 * @param strip: LED strip
 * @param index: index of pixel to set
 * @param hue: hue part of color (0 - 360)
 * @param saturation: saturation part of color (0 - 255, rescaled from 0 - 1. e.g. saturation = 0.5, rescaled to 127)
 * @param value: value part of color (0 - 255, rescaled from 0 - 1. e.g. value = 0.5, rescaled to 127)
 *
 * @return
 *      - ESP_OK: Set HSV color for a specific pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set HSV color for a specific pixel failed because of an invalid argument
 *      - ESP_FAIL: Set HSV color for a specific pixel failed because other error occurred
 */
esp_err_t led_strip_set_pixel_hsv(led_strip_handle_t strip, uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value);

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
