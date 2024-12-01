/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_camera.h"
#include "quirc.h"

static const char *TAG = "example";

lv_obj_t *camera_canvas = NULL;

void grayscale_to_rgb565(uint8_t *grayscale_buf, uint8_t *rgb565_buf, uint16_t length)
{
    uint8_t reduced = 0;
    uint32_t rgb565_counter = 0;
    uint16_t k = 0;
    for (k = 0; k < length; k++) {
        reduced = (grayscale_buf[k] >> 3) & 0b00011111;
        rgb565_buf[rgb565_counter] = ((reduced << 3) | (reduced >> 2));
        rgb565_counter++;
        rgb565_buf[rgb565_counter] = ((reduced << 6) | (reduced));
        rgb565_counter++;
    }
}

static void decode_task(void *args)
{
    bsp_i2c_init();
    bsp_display_start();
    bsp_display_backlight_on(); // Set display brightness to 100%

    // Initialize the camera
    camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
    camera_config.pixel_format = PIXFORMAT_GRAYSCALE; // required by quirc
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        exit(1);
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_contrast(s, 2);
    ESP_LOGI(TAG, "Camera Init done");

    // Create LVGL canvas for camera image
    bsp_display_lock(0);
    camera_canvas = lv_canvas_create(lv_scr_act());

    assert(camera_canvas);
    lv_obj_center(camera_canvas);
    bsp_display_unlock();

    camera_fb_t *pic = esp_camera_fb_get();
    uint8_t *buf_shown = (uint8_t *)lv_mem_alloc(pic->len * 2); // double the size because the canvas requires a RGB565, i.e. 2 bytes per pixel
    esp_camera_fb_return(pic);

    /* Initializing the quirc handle */
    struct quirc *q = quirc_new();
    if (!q) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        exit(1);
    }
    if (quirc_resize(q, pic->width, pic->height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate video  memory\n");
        exit(1);
    }

    struct quirc_code code;
    struct quirc_data data;
    quirc_decode_error_t q_err;
    uint16_t num_codes;

    while (1) {
        pic = esp_camera_fb_get();

        if (pic) {
            bsp_display_lock(0);
            grayscale_to_rgb565(pic->buf, buf_shown, pic->len);
            lv_canvas_set_buffer(camera_canvas, buf_shown, pic->width, pic->height, LV_IMG_CF_TRUE_COLOR);
            bsp_display_unlock();

            memcpy(quirc_begin(q, NULL, NULL), pic->buf, pic->len);
            quirc_end(q);

            num_codes = quirc_count(q);
            for (uint16_t i = 0; i < num_codes; i++) {

                quirc_extract(q, i, &code);
                /* Decoding stage */
                q_err = quirc_decode(&code, &data);
                if (q_err != 0) {
                    printf("%d/%d] DECODE FAILED: %s\n", i + 1, num_codes, quirc_strerror(q_err));
                } else {
                    printf("%d/%d] DATA: %s\n", i + 1, num_codes, data.payload);
                }
            }

            esp_camera_fb_return(pic);
        } else {
            ESP_LOGE(TAG, "Get frame failed");
            exit(1);
        }

    }
}
void app_main(void)
{
    xTaskCreatePinnedToCore(decode_task, TAG, 40 * 1024, NULL, 6, NULL, 0);
}

