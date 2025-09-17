/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_sh8601.h"
#include "driver/spi_master.h"
#include "thorvg_capi.h"

static const char *TAG = "example";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_PIN_NUM_LCD_CS      12
#define EXAMPLE_PIN_NUM_LCD_PCLK    11
#define EXAMPLE_PIN_NUM_LCD_DATA0   4
#define EXAMPLE_PIN_NUM_LCD_DATA1   5
#define EXAMPLE_PIN_NUM_LCD_DATA2   6
#define EXAMPLE_PIN_NUM_LCD_DATA3   7

#define EXAMPLE_LCD_PCLK_HZ         (40 * 1000 * 1000)
#define EXAMPLE_LCD_BIT_PER_PIXEL   16  // RGB565

#define EXAMPLE_LCD_SPI_HOST        SPI2_HOST

#define EXAMPLE_FS_MOUNT_POINT     "/storage"
#define EXAMPLE_LOTTIE_FILENAME     EXAMPLE_FS_MOUNT_POINT"/emoji-animation.json"
#define EXAMPLE_LOTTIE_SIZE_HOR     320
#define EXAMPLE_LOTTIE_SIZE_VER     320


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

static void argb888_to_rgb565(const uint32_t *in, uint16_t *out, size_t num_pixels)
{
    for (size_t i = 0; i < num_pixels; ++i) {
        uint32_t argb = in[i];
        uint8_t r = (argb >> 16) & 0xFF;
        uint8_t g = (argb >> 8) & 0xFF;
        uint8_t b = argb & 0xFF;
        // Convert to RGB565
        uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        rgb565 = (rgb565 >> 8) | (rgb565 << 8);
        out[i] = rgb565;
    }
}

static void play_lottie(esp_lcd_panel_handle_t lcd_panel, uint32_t *canvas_buf_argb888, uint16_t *canvas_buf_rgb565)
{
    // Initialize ThorVG engine
    if (tvg_engine_init(TVG_ENGINE_SW, 0) != TVG_RESULT_SUCCESS) {
        printf("Failed to initialize ThorVG engine\n");
        abort();
    }

    // Create a canvas. Here, using SW engine and ARGB8888 buffer format
    Tvg_Canvas *canvas = tvg_swcanvas_create();
    assert(canvas);
    tvg_swcanvas_set_target(canvas, canvas_buf_argb888, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_VER, TVG_COLORSPACE_ARGB8888);
    // flush the background with black
    esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_VER, canvas_buf_rgb565);

    // Create an animation object
    Tvg_Animation *animation = tvg_animation_new();
    // Get the picture object from animation
    Tvg_Paint *picture = tvg_animation_get_picture(animation);

    // Load the Lottie file (JSON)
    if (tvg_picture_load(picture, EXAMPLE_LOTTIE_FILENAME) != TVG_RESULT_SUCCESS) {
        printf("Problem with loading a lottie file\n");
        abort();
    }
    // Resize the picture
    tvg_picture_set_size(picture, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_VER);
    // Push the animation to the canvas
    tvg_canvas_push(canvas, picture);

    // Play the animation frame by frame
    float f_total;
    float f = 0;
    tvg_animation_get_total_frame(animation, &f_total);
    while (f < f_total) {
        tvg_animation_get_frame(animation, &f);
        f++;
        tvg_animation_set_frame(animation, f);
        tvg_canvas_update(canvas);
        // Draw the canvas (renders to the buffer)
        tvg_canvas_draw(canvas);
        // Sync to ensure drawing is completed
        tvg_canvas_sync(canvas);

        // wait for the last flush is finished before reusing the canvas buffer
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Convert the buffer from ARGB8888 to RGB565 and flush to the display
        argb888_to_rgb565(canvas_buf_argb888, canvas_buf_rgb565, EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER);
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_VER, canvas_buf_rgb565);
    }

    // Cleanup
    tvg_animation_del(animation);
    tvg_canvas_destroy(canvas);
    tvg_engine_term(TVG_ENGINE_SW);
}

static esp_err_t example_init_fs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = EXAMPLE_FS_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

static bool example_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    TaskHandle_t task_handle = (TaskHandle_t)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    vTaskNotifyGiveFromISR(task_handle, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void app_main(void)
{
    // allocate the canvas buffer(s) from PSRAM
    uint32_t *canvas_buf_argb888 = heap_caps_calloc(EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER, sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    uint16_t *canvas_buf_rgb565 = heap_caps_calloc(EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    assert(canvas_buf_argb888 && canvas_buf_rgb565);

    // lottie files are saved in the filesystem, we need to initialize the file system first
    ESP_ERROR_CHECK(example_init_fs());

    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK,
        .data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0,
        .data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1,
        .data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2,
        .data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3,
        .max_transfer_sz = EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER * 3,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .dc_gpio_num = -1, // no D/C pin for SH8601
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PCLK_HZ,
        .trans_queue_depth = 20,
        .lcd_cmd_bits = 32,  // according to SH8601 spec
        .lcd_param_bits = 8, // according to SH8601 spec
        .flags = {
            .quad_mode = true, // QSPI mode
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(EXAMPLE_LCD_SPI_HOST, &io_config, &io_handle));

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_on_color_trans_done,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, xTaskGetCurrentTaskHandle());

    esp_lcd_panel_handle_t lcd_panel = NULL;
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1, // SH8601 support many interfaces, we select QSPI here
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &lcd_panel));

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    while (1) {
        play_lottie(lcd_panel, canvas_buf_argb888, canvas_buf_rgb565);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
