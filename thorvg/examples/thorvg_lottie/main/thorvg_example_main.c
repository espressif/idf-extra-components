/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * ThorVG Lottie animation example.
 */

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_pthread.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "thorvg_capi.h"
#include "example_lcd.h"

static const char *TAG = "example";

#define EXAMPLE_FS_MOUNT_POINT      "/storage"
#define EXAMPLE_LOTTIE_FILENAME     EXAMPLE_FS_MOUNT_POINT "/emoji-animation.json"
// Keep the canvas size consistent with the panel area being updated.
#define EXAMPLE_LOTTIE_SIZE_HOR     368
#define EXAMPLE_LOTTIE_SIZE_VER     448

// Convert ThorVG's ARGB8888 output to the panel's byte-swapped RGB565 format.
static void argb888_to_rgb565(const uint32_t *in, uint16_t *out, size_t num_pixels)
{
    for (size_t i = 0; i < num_pixels; ++i) {
        uint32_t argb = in[i];
        uint8_t r = (argb >> 16) & 0xFF;
        uint8_t g = (argb >> 8) & 0xFF;
        uint8_t b = argb & 0xFF;
        uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        rgb565 = (rgb565 >> 8) | (rgb565 << 8);
        out[i] = rgb565;
    }
}

typedef struct {
    example_lcd_t *lcd;
    uint32_t *canvas_buf_argb888;
    uint16_t *canvas_buf_rgb565;
} lottie_render_ctx_t;

// Render one frame through the ThorVG-to-LCD pipeline.
static void render_lottie_frame(Tvg_Canvas canvas, Tvg_Animation animation,
                                lottie_render_ctx_t *ctx, float frame)
{
    // Frame 0 is already selected after loading and is not an error.
    Tvg_Result frame_result = tvg_animation_set_frame(animation, frame);
    if (frame_result != TVG_RESULT_SUCCESS && frame_result != TVG_RESULT_INSUFFICIENT_CONDITION) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    ESP_ERROR_CHECK(tvg_canvas_update(canvas) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(tvg_canvas_draw(canvas, false) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    // The ARGB8888 buffer must not be read before rendering has completed.
    ESP_ERROR_CHECK(tvg_canvas_sync(canvas) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);

    // Wait before reusing the buffer submitted by the previous LCD transfer.
    xSemaphoreTake(ctx->lcd->flush_done_sem, portMAX_DELAY);
    argb888_to_rgb565(ctx->canvas_buf_argb888, ctx->canvas_buf_rgb565,
                      EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(ctx->lcd->panel, 0, 0,
                                              EXAMPLE_LOTTIE_SIZE_HOR,
                                              EXAMPLE_LOTTIE_SIZE_VER,
                                              ctx->canvas_buf_rgb565));
}

// Keep rendering off the application task and replay the animation.
static void *lottie_render_thread(void *arg)
{
    lottie_render_ctx_t *ctx = (lottie_render_ctx_t *)arg;

    // ThorVG creates std::thread workers; keep their stacks separate and smaller.
    esp_pthread_cfg_t worker_cfg = esp_pthread_get_default_config();
    worker_cfg.stack_size = 8 * 1024;
    ESP_ERROR_CHECK(esp_pthread_set_cfg(&worker_cfg));

    // One worker provides the best balance for the current animation and target.
    ESP_ERROR_CHECK(tvg_engine_init(1) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    Tvg_Canvas canvas = tvg_swcanvas_create(TVG_ENGINE_OPTION_DEFAULT);
    ESP_ERROR_CHECK(canvas ? ESP_OK : ESP_ERR_NO_MEM);
    // Stride is measured in pixels, not bytes.
    ESP_ERROR_CHECK(tvg_swcanvas_set_target(canvas, ctx->canvas_buf_argb888,
                                            EXAMPLE_LOTTIE_SIZE_HOR,
                                            EXAMPLE_LOTTIE_SIZE_HOR,
                                            EXAMPLE_LOTTIE_SIZE_VER,
                                            TVG_COLORSPACE_ARGB8888) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);

    Tvg_Animation animation = tvg_lottie_animation_new();
    ESP_ERROR_CHECK(animation ? ESP_OK : ESP_ERR_NO_MEM);

    Tvg_Paint picture = tvg_animation_get_picture(animation);
    ESP_ERROR_CHECK(tvg_picture_load(picture, EXAMPLE_LOTTIE_FILENAME) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(tvg_picture_set_size(picture, EXAMPLE_LOTTIE_SIZE_HOR,
                                         EXAMPLE_LOTTIE_SIZE_VER) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(tvg_canvas_add(canvas, picture) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);

    float total_frames = 0;
    float duration = 0;
    ESP_ERROR_CHECK(tvg_animation_get_total_frame(animation, &total_frames) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(tvg_animation_get_duration(animation, &duration) == TVG_RESULT_SUCCESS ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(duration > 0 && total_frames > 0 ? ESP_OK : ESP_FAIL);

    const int64_t start_time_us = esp_timer_get_time();
    int last_frame = -1;
    while (1) {
        // Use elapsed time rather than render speed to keep playback rate stable.
        const float elapsed = (float)(esp_timer_get_time() - start_time_us) / 1000000.0f;
        const float animation_time = fmodf(elapsed, duration);
        const int frame = (int)floorf(animation_time * total_frames / duration);
        if (frame == last_frame) {
            vTaskDelay(1);
            continue;
        }

        render_lottie_frame(canvas, animation, ctx, (float)frame);
        last_frame = frame;
    }
}

// Mount the partition containing the bundled Lottie file.
static esp_err_t example_init_fs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = EXAMPLE_FS_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ESP_ERROR_CHECK(esp_littlefs_info(conf.partition_label, &total, &used));
    ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
    return ESP_OK;
}

void app_main(void)
{
    const size_t pixel_count = EXAMPLE_LOTTIE_SIZE_HOR * EXAMPLE_LOTTIE_SIZE_VER;
    // ThorVG renders to ARGB8888; the LCD receives a converted RGB565 frame.
    uint32_t *canvas_buf_argb888 = heap_caps_calloc(pixel_count, sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    uint16_t *canvas_buf_rgb565 = heap_caps_calloc(pixel_count, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    ESP_ERROR_CHECK(!canvas_buf_argb888 || !canvas_buf_rgb565 ? ESP_ERR_NO_MEM : ESP_OK);

    ESP_ERROR_CHECK(example_init_fs());

    // LCD details are isolated in example_lcd.c so this file focuses on ThorVG.
    static example_lcd_t lcd = {};
    ESP_ERROR_CHECK(example_init_lcd(&lcd, EXAMPLE_LOTTIE_SIZE_HOR, EXAMPLE_LOTTIE_SIZE_VER));

    // Keep the context alive after app_main() returns.
    static lottie_render_ctx_t render_ctx = {};
    render_ctx.lcd = &lcd;
    render_ctx.canvas_buf_argb888 = canvas_buf_argb888;
    render_ctx.canvas_buf_rgb565 = canvas_buf_rgb565;

    // ThorVG rendering uses a dedicated task with a larger stack.
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "lottie_render";
    cfg.stack_size = 30 * 1024;
    ESP_ERROR_CHECK(esp_pthread_set_cfg(&cfg));

    pthread_t thread;
    int ret = pthread_create(&thread, NULL, lottie_render_thread, &render_ctx);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create render thread: %d", ret);
        abort();
    }
    pthread_detach(thread);
}
