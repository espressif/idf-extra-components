/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of LED strip handle
 */
typedef struct led_strip_t *led_strip_handle_t;

/**
 * @brief Type of LED strip group handle
 */
typedef struct led_strip_group_t *led_strip_group_handle_t;

/**
 * @brief LED strip model
 * @note Different led model may have different timing parameters, so we need to distinguish them.
 */
typedef enum {
    LED_MODEL_WS2812, /*!< LED strip model: WS2812 */
    LED_MODEL_SK6812, /*!< LED strip model: SK6812 */
    LED_MODEL_WS2811, /*!< LED strip model: WS2811 */
    LED_MODEL_CUSTOM, /*!< Custom LED strip model. Only used for RMT backend. The timings can be specified by the `led_strip_encoder_timings_t` */
    LED_MODEL_INVALID /*!< Invalid LED strip model */
} led_model_t;

/**
 * @brief LED strip encoder timings.
 * @note The logic timings are in nanoseconds and the reset timings is in microseconds.
 */
typedef struct {
    uint32_t t0h; /*!< High time for 0 bit, nanoseconds */
    uint32_t t1h; /*!< High time for 1 bit, nanoseconds */
    uint32_t t0l; /*!< Low time for 0 bit, nanoseconds */
    uint32_t t1l; /*!< Low time for 1 bit, nanoseconds */
    uint32_t reset; /*!< Reset time, microseconds */
} led_strip_encoder_timings_t;

/**
 * @brief LED color component format
 * @note The format is used to specify the order of color components in each pixel, also the number of color components.
 */
typedef union {
    struct format_layout {
        uint32_t r_pos: 2;          /*!< Position of the red channel in the color order: 0~3 */
        uint32_t g_pos: 2;          /*!< Position of the green channel in the color order: 0~3 */
        uint32_t b_pos: 2;          /*!< Position of the blue channel in the color order: 0~3 */
        uint32_t w_pos: 2;          /*!< Position of the white channel in the color order: 0~3 */
        uint32_t reserved: 21;      /*!< Reserved */
        uint32_t num_components: 3; /*!< Number of color components per pixel: 3 or 4. If set to 0, it will fallback to 3 */
    } format;                       /*!< Format layout */
    uint32_t format_id;             /*!< Format ID */
} led_color_component_format_t;

/// Helper macros to set the color component format
#define LED_STRIP_COLOR_COMPONENT_FMT_GRB (led_color_component_format_t){.format = {.r_pos = 1, .g_pos = 0, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 3}}
#define LED_STRIP_COLOR_COMPONENT_FMT_GRBW (led_color_component_format_t){.format = {.r_pos = 1, .g_pos = 0, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 4}}
#define LED_STRIP_COLOR_COMPONENT_FMT_RGB (led_color_component_format_t){.format = {.r_pos = 0, .g_pos = 1, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 3}}
#define LED_STRIP_COLOR_COMPONENT_FMT_RGBW (led_color_component_format_t){.format = {.r_pos = 0, .g_pos = 1, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 4}}

/**
 * @brief LED Strip common configurations
 *        The common configurations are not specific to any backend peripheral.
 */
typedef struct {
    int strip_gpio_num;           /*!< GPIO number that used by LED strip */
    uint32_t max_leds;            /*!< Maximum number of LEDs that can be controlled in a single strip */
    led_model_t led_model;        /*!< Specifies the LED strip model (e.g., WS2812, SK6812) */
    led_color_component_format_t color_component_format; /*!< Specifies the order of color components in each pixel.
                                                              Use helper macros like `LED_STRIP_COLOR_COMPONENT_FMT_GRB` to set the format */
    led_strip_encoder_timings_t timings; /*!< Encoder timings, only valid for RMT backend */
    /*!< LED strip extra driver flags */
    struct led_strip_extra_flags {
        uint32_t invert_out: 1; /*!< Invert output signal */
    } flags; /*!< Extra driver flags */
} led_strip_config_t;

#ifdef __cplusplus
}
#endif
