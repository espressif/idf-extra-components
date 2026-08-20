/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t flush_done_sem;
} example_lcd_t;

esp_err_t example_init_lcd(example_lcd_t *lcd, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif
