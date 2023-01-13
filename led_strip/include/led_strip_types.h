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

enum led_type_t { LED_TYPE_WS2812 = 0, LED_TYPE_SK6812 };

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
    enum led_type_t led_type;
    struct {
        uint32_t invert_out: 1; /*!< Invert output signal */
    } flags;
} led_strip_config_t;

#ifdef __cplusplus
}
#endif
