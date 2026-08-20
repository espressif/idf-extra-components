/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_sh8601.h"
#include "example_lcd.h"

static const char *TAG = "example_lcd";

#define EXAMPLE_PIN_NUM_LCD_CS      12
#define EXAMPLE_PIN_NUM_LCD_PCLK    11
#define EXAMPLE_PIN_NUM_LCD_DATA0   4
#define EXAMPLE_PIN_NUM_LCD_DATA1   5
#define EXAMPLE_PIN_NUM_LCD_DATA2   6
#define EXAMPLE_PIN_NUM_LCD_DATA3   7

#define EXAMPLE_LCD_PCLK_HZ         (20 * 1000 * 1000)
#define EXAMPLE_LCD_BIT_PER_PIXEL   16
#define EXAMPLE_LCD_SPI_HOST        SPI2_HOST

// SH8601 initialization commands: {command, parameters, length, delay (ms)}
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *event_data,
                                void *user_ctx)
{
    (void) panel_io;
    (void) event_data;
    SemaphoreHandle_t flush_done_sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(flush_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

esp_err_t example_init_lcd(example_lcd_t *lcd, uint32_t width, uint32_t height)
{
    const size_t pixel_count = (size_t)width * height;
    spi_bus_config_t bus_config = {
        .sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK,
        .data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0,
        .data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1,
        .data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2,
        .data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3,
        .max_transfer_sz = pixel_count * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EXAMPLE_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO),
                        TAG, "Failed to initialize SPI bus");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PCLK_HZ,
        .trans_queue_depth = 20,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = {
            .quad_mode = true,
            // Use the PSRAM color buffer directly for DMA instead of a temporary copy.
            .psram_dma_direct = true,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(EXAMPLE_LCD_SPI_HOST, &io_config, &io_handle),
                        TAG, "Failed to create LCD panel IO");

    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &lcd->panel),
                        TAG, "Failed to create SH8601 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(lcd->panel), TAG, "Failed to reset LCD panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(lcd->panel), TAG, "Failed to initialize LCD panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(lcd->panel, true), TAG, "Failed to enable LCD panel");

    lcd->flush_done_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(lcd->flush_done_sem, ESP_ERR_NO_MEM, TAG, "Failed to create flush semaphore");
    xSemaphoreGive(lcd->flush_done_sem);

    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = on_color_trans_done,
    };
    return esp_lcd_panel_io_register_event_callbacks(io_handle, &callbacks, lcd->flush_done_sem);
}
