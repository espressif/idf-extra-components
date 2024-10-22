/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED strip model
 * @note Different led model may have different timing parameters, so we need to distinguish them.
 */
typedef enum {
    LED_MODEL_WS2812, /*!< LED strip model: WS2812 */
    LED_MODEL_SK6812, /*!< LED strip model: SK6812 */
    LED_MODEL_INVALID /*!< Invalid LED strip model */
} led_model_t;

/**
 * @brief LED strip handle
 */
typedef struct led_strip_t *led_strip_handle_t;

/**
 * @brief LED Strip Configuration
 */
typedef struct {
    int strip_gpio_num;      /*!< GPIO number that used by LED strip */
    uint32_t max_leds;       /*!< Maximum LEDs in a single strip */
    uint8_t bytes_per_pixel; /*!< bytes per LED pixel. Should be 3 or 4 */
    led_model_t led_model;   /*!< LED model */
    uint8_t pixel_order; /*! The order of the pixel color.
                             Use help macro LED_STRIP_SET_RGB_ORDER or LED_STRIP_SET_RGBW_ORDER to set.
                             Not set or set to 0 if the default order is used. */
    struct {
        uint32_t invert_out: 1; /*!< Invert output signal */
    } flags;                    /*!< Extra driver flags */
} led_strip_config_t;

#ifdef __cplusplus
}
#endif
