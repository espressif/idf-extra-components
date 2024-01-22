/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RGB_QEMU_BPP_32 = 32,
    RGB_QEMU_BPP_16 = 16,
} esp_lcd_rgb_qemu_bpp_t;
/**
 * @brief QEMU RGB panel configuration structure
 */
typedef struct {
    uint32_t width;             /*!< Width of the graphical window in pixels */
    uint32_t height;            /*!< Height of the graphical window in pixels */
    esp_lcd_rgb_qemu_bpp_t bpp;                /*!< BPP - bit per pixel*/
} esp_lcd_rgb_qemu_config_t;

/**
 * @brief Create QEMU RGB panel
 *
 * @param[in] rgb_config QEMU RGB panel configuration
 * @param[out] ret_panel Returned panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG: Creation failed because of invalid argument, check the configuration parameter
 *      - ESP_ERR_NO_MEM: Creation failed because of the lack of free memory in the heap
 *      - ESP_ERR_NOT_SUPPORTED: Creation failed because this API must only be used within a QEMU virtual machine
 *      - ESP_OK: Panel created successfully
 */
esp_err_t esp_lcd_new_rgb_qemu(const esp_lcd_rgb_qemu_config_t *rgb_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Get the address of the frame buffer for the QEMU RGB panel
 *
 * @param[in] panel QEMU RGB panel handle, returned from `esp_lcd_new_rgb_qemu`
 * @param[out] fb Returned address of the frame buffer
 * @return
 *      - ESP_OK: Frame buffer returned successfully
 */
esp_err_t esp_lcd_rgb_qemu_get_frame_buffer(esp_lcd_panel_handle_t panel, void **fb);

/**
 * @brief Manually trigger once transmission of the frame buffer to the panel
 *
 * @param[in] panel QEMU RGB panel handle, returned from `esp_lcd_new_rgb_qemu`
 * @returns ESP_OK unconditionally
 */
esp_err_t esp_lcd_rgb_qemu_refresh(esp_lcd_panel_handle_t panel);

#ifdef __cplusplus
}
#endif
