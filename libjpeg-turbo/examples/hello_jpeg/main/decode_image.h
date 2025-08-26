/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once
#include <stdint.h>
#include "esp_err.h"

#define IMAGE_W 320
#define IMAGE_H 240

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode the jpeg ``image.jpg`` embedded into the program file into pixel data and measure the performance.
 *
 * @param pixels A pointer to a pointer for an array of rows, which themselves are an array of pixels.
 * @return - ESP_OK on successful decode
 */
esp_err_t decode_image(uint16_t **pixels);

#ifdef __cplusplus
}
#endif
