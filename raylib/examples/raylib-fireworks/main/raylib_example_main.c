/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"

#include "raylib.h"
#include "esp_raylib.h"
#include "fireworks.h"

#define LCD_HOST        SPI3_HOST
#define LCD_MOSI        GPIO_NUM_6
#define LCD_SCLK        GPIO_NUM_7
#define LCD_CS          GPIO_NUM_5
#define LCD_DC          GPIO_NUM_4
#define LCD_RST         GPIO_NUM_48
#define LCD_BACKLIGHT   GPIO_NUM_45

#define LCD_H_RES       320
#define LCD_V_RES       240
#define LCD_PIXEL_CLOCK (40 * 1000 * 1000)
#define LCD_BAND_ROWS   48

/* The context shared between the transfer-done ISR and flush_cb. */
typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_raylib_display_handle_t display;
} panel_ctx_t;

/* Two 48-row bands ping-pong with SPI DMA:
 *
 *   memcpy band A  →  panel_flush(A) starts DMA  →  memcpy band B
 *                     (DMA still reading A)         panel_flush(B) ...
 *
 * Return as soon as draw_bitmap() has queued the transfer. */
static void panel_flush(esp_raylib_display_handle_t display, const void *pixels,
                        uint16_t x, uint16_t y, uint16_t w, uint16_t h, void *user_data)
{
    (void)display;
    panel_ctx_t *ctx = user_data;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(ctx->panel, x, y, x + w, y + h, pixels));
}

/* SPI finished reading the last panel_flush band. */
static IRAM_ATTR bool color_trans_done_cb(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)io;
    (void)edata;
    panel_ctx_t *ctx = user_ctx;
    esp_raylib_flush_ready(ctx->display);
    return pdFALSE;
}

static esp_lcd_panel_handle_t display_init(panel_ctx_t *ctx)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = LCD_MOSI,
        .sclk_io_num = LCD_SCLK,
        /* unused pins must be -1: a zeroed field is a valid GPIO */
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_BAND_ROWS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS,
        .dc_gpio_num = LCD_DC,
        .pclk_hz = LCD_PIXEL_CLOCK,
        .trans_queue_depth = 10,   /* >= 2: next draw_bitmap can queue while the previous DMA still runs */
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,   /* match CONFIG_RAYLIB_FRAMEBUFFER_FORMAT_RGB565 */
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    const gpio_config_t backlight = {
        .pin_bit_mask = (1ULL << LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&backlight));
    gpio_set_level(LCD_BACKLIGHT, 1);

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = color_trans_done_cb
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io, &cbs, ctx));

    return panel;
}

static void raylib_task(void *arg)
{
    (void)arg;

    InitWindow(LCD_H_RES, LCD_V_RES, NULL);
    SetTargetFPS(60);
    SetRandomSeed((unsigned)esp_timer_get_time());
    fireworks_init(LCD_H_RES, LCD_V_RES);

    while (!WindowShouldClose()) {
        /* raw frame time: fireworks_update paces itself with a fixed-step
         * accumulator, so the show stays in sync with wall time */
        fireworks_update(GetFrameTime());

        BeginDrawing();
        fireworks_draw((float)GetTime());
        EndDrawing();
    }

    CloseWindow();
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* Static: lives for the whole program and is read by the ISR. */
    static panel_ctx_t ctx = {};

    ctx.panel = display_init(&ctx);

    const esp_raylib_display_config_t display_cfg = {
        .chunk_rows = LCD_BAND_ROWS,
        .swap_bytes = true,    /* ST7789 over SPI takes RGB565 MSB first */
        .mirror = ESP_RAYLIB_MIRROR_Y,  /* ST7789 scans row 0 as the panel top */
        .flush_cb = panel_flush,
        .user_data = &ctx,
    };
    ctx.display = esp_raylib_add_display(&display_cfg);
    ESP_ERROR_CHECK(ctx.display != NULL ? ESP_OK : ESP_FAIL);

    /* The rasteriser needs deep stack on the calling task; 64 KiB is a comfortable start. */
    ESP_ERROR_CHECK(xTaskCreate(raylib_task, "raylib", 64 * 1024,
                                NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_FAIL);
}
