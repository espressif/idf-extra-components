/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_PIXEL_FORMAT_GRB,
    LED_PIXEL_FORMAT_GRBW
} led_pixel_format_t;

typedef enum {
    LED_MODEL_WS2812,
    LED_MODEL_SK6812
} led_model_t;

/**
 * @brief LED strip handle
 */
typedef struct led_strip_t *led_strip_handle_t;

/**
 * @brief LED Strip Configuration
 */
typedef struct {
    uint32_t strip_gpio_num; /*!< GPIO number that used by LED strip */
    uint32_t max_leds;       /*!< Maximum LEDs in a single strip */
    led_pixel_format_t led_pixel_format;
    led_model_t led_model;
    struct {
        uint32_t invert_out: 1; /*!< Invert output signal */
    } flags;
} led_strip_config_t;

#ifdef __cplusplus
}
#endif
