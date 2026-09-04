/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for the screenshot panel driver
 *
 * Supported color formats (`esp_color_fourcc_t`):
 *  - ESP_COLOR_FOURCC_RGB16:    RGB565, little-endian uint16_t
 *  - ESP_COLOR_FOURCC_RGB16_BE: RGB565, big-endian
 *  - ESP_COLOR_FOURCC_RGB24:    RGB888, bytes in R, G, B order
 *  - ESP_COLOR_FOURCC_BGR24:    RGB888, bytes in B, G, R order
 *                               (LVGL RGB888 / most ESP-IDF RGB888 paths)
 *  - ESP_COLOR_FOURCC_BGRA32:   ARGB8888, bytes in B, G, R, A order
 */
typedef struct {
    int width;                          /*!< Panel width in pixels */
    int height;                         /*!< Panel height in pixels */
    esp_color_fourcc_t color_format;    /*!< Framebuffer color format */

    /**
     * Optional real LCD panel to wrap. If not NULL, the screenshot panel
     * takes ownership of this handle: every panel operation is forwarded to
     * it, and `esp_lcd_panel_del()` on the screenshot panel also destroys
     * the wrapped panel. Do not use or delete the original handle afterwards.
     * If NULL, the driver acts as a purely virtual panel.
     */
    esp_lcd_panel_handle_t real_panel;
} esp_lcd_screenshot_config_t;

/**
 * @brief Create a virtual screenshot panel driver
 *
 * The driver accepts draw calls like a real LCD panel and keeps a copy of
 * the latest frame. The capture is the pixel data the GUI submitted; set
 * width and height to the GUI resolution.
 *
 * If config->real_panel is not NULL, this driver takes ownership of that
 * handle. Use only the screenshot panel afterwards. `esp_lcd_panel_del()`
 * on the screenshot panel also destroys the wrapped panel.
 *
 * Use esp_lcd_screenshot_dump_base64() or esp_lcd_screenshot_save_png() to
 * output the captured framebuffer.
 *
 * @param config    Panel configuration
 * @param ret_panel Returned panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if config or ret_panel is NULL, dimensions overflow, or color_format is not supported
 *      - ESP_ERR_NO_MEM if memory allocation for the panel or framebuffer fails
 */
esp_err_t esp_lcd_new_screenshot_panel(const esp_lcd_screenshot_config_t *config,
                                       esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Dump the current framebuffer content as base64 to a FILE stream
 *
 * Outputs the raw pixel data between FRAMEBUFFER_BEGIN and FRAMEBUFFER_END
 * markers, encoded as base64. The FRAMEBUFFER_BEGIN line is
 * "FRAMEBUFFER_BEGIN <width> <height> <fourcc>", where <fourcc> is the
 * 4-character code (e.g. BGR3, RGBL). Each payload line is prefixed with
 * "FB_BASE64 ".
 *
 * This is meant to be sent over the serial console and reassembled on the
 * host, e.g. by a pytest script. The dump yields between encoded lines so
 * the task watchdog is not triggered by a long blocking console write. Call
 * this only after the GUI has finished the frame you want; the panel API
 * does not provide a generic frame boundary, and drawing is not paused.
 *
 * @param panel  Panel handle created by esp_lcd_new_screenshot_panel()
 * @param stream Output stream (e.g. stdout). If NULL, uses stdout.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if panel is NULL or is not a screenshot panel
 */
esp_err_t esp_lcd_screenshot_dump_base64(esp_lcd_panel_handle_t panel, FILE *stream);

/**
 * @brief Save the current framebuffer content to a file in PNG format
 *
 * Converts one scanline at a time and feeds it to libpng (`png_write_row`),
 * so no extra full-frame RGB buffer is allocated. Call this only after the
 * GUI has finished the frame you want; drawing is not paused. If writing
 * fails after the file has been created, the incomplete file is removed.
 *
 * @param panel    Panel handle created by esp_lcd_new_screenshot_panel()
 * @param filepath Path of the PNG file to write (e.g. a path on a mounted
 *                 filesystem like LittleFS or SD card)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if filepath is NULL, or panel is NULL or is not a screenshot panel
 *      - ESP_ERR_NO_MEM if memory allocation for the PNG encoder or scanline buffer fails
 *      - ESP_FAIL if filepath cannot be opened or the PNG file cannot be written
 */
esp_err_t esp_lcd_screenshot_save_png(esp_lcd_panel_handle_t panel, const char *filepath);

#ifdef __cplusplus
}
#endif
