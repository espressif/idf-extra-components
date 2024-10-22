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

/**
 * @brief LED strip pixel order index
 */
typedef enum {
    LED_PIXEL_INDEX_RED,     /*!< Red pixel index */
    LED_PIXEL_INDEX_GREEN,   /*!< Green pixel index */
    LED_PIXEL_INDEX_BLUE,    /*!< Blue pixel index */
    LED_PIXEL_INDEX_WHITE,   /*!< White pixel index */
    LED_PIXEL_INDEX_MAX      /*!< Max pixel index */
} led_pixel_order_index_t;

/**
 * @brief Set LED color order
 *
 * @param led_pixel_offset Each pixel's offset
 * @param pixel_order `pixel_order` parameter in LED strip configuration
 * @param bytes_per_pixel bytes per pixel
 * @return
 *      - ESP_OK: Set LED color order successfully
 *      - ESP_ERR_INVALID_ARG: Set LED color order failed because of invalid argument
 */
esp_err_t led_strip_set_color_order(uint8_t *led_pixel_offset, const uint8_t pixel_order, const uint8_t bytes_per_pixel);

#ifdef __cplusplus
}
#endif