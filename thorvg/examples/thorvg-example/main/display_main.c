/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/esp-bsp.h"
#include "driver/ppa.h"
#include "thorvg_capi.h"

static const char *TAG = "example";

static void play_tick_new(esp_timer_handle_t *tm);
static void play_tick_del(esp_timer_handle_t tm);
static uint32_t play_tick_get(void);
static uint32_t play_tick_elaps(uint32_t prev_tick);

static void capi_loop_task(void *arg);
static esp_err_t capi_create_lottie(ppa_client_handle_t ppa_handle, bsp_lcd_handles_t *lcd_panel);
static esp_err_t argb888_to_rgb565_ppa(ppa_client_handle_t ppa_handle, const uint32_t *in, uint16_t *out);

/* SPIFFS mount root */
#define FS_MNT_PATH             BSP_SPIFFS_MOUNT_POINT

#define LOTTIE_SIZE_HOR         (320)
#define LOTTIE_SIZE_VER         (320)

#define EXPECTED_FPS            (20)
#define LOTTIE_FILENAME         FS_MNT_PATH"/emoji-animation.json"

static uint32_t sys_time = 0;
static volatile uint8_t tick_irq_flag;

void app_main(void)
{
    static bsp_lcd_handles_t lcd_panel;

    bsp_spiffs_mount();

    /* Initialize display */
    bsp_display_new_with_handles(NULL, &lcd_panel);

    bsp_display_backlight_on();

    BaseType_t res = xTaskCreate(capi_loop_task, "thorvg task", 60 * 1024, (void *)&lcd_panel, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Create thorvg task fail!");
    }
}

void capi_loop_task(void *arg)
{
    bsp_lcd_handles_t *lcd_panel = (bsp_lcd_handles_t *)arg;

    ppa_client_handle_t ppa_client_srm_handle = NULL;
    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ppa_register_client(&ppa_client_config, &ppa_client_srm_handle);

    if (ppa_client_srm_handle) {
        while (1) {
            capi_create_lottie(ppa_client_srm_handle, lcd_panel);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    } else {
        ESP_LOGE(TAG, "ppa_register_client failed");
    }

    if (ppa_client_srm_handle) {
        ppa_unregister_client(ppa_client_srm_handle);
    }

    vTaskDelete(NULL);
}

static esp_err_t capi_create_lottie(ppa_client_handle_t ppa_handle, bsp_lcd_handles_t *lcd_panel)
{
    esp_err_t ret = ESP_OK;

    Tvg_Result tvg_res = TVG_RESULT_SUCCESS;
    Tvg_Result tvg_engine = TVG_RESULT_INSUFFICIENT_CONDITION;

    static uint32_t reac_color = 0x00;

    uint32_t *canvas_buf_888 = NULL;
    uint16_t *canvas_buf_565 = NULL;

    Tvg_Animation *animation = NULL;
    Tvg_Canvas *canvas = NULL;
    esp_timer_handle_t play_timer = NULL;

    play_tick_new(&play_timer);

    canvas_buf_888 = heap_caps_aligned_calloc(64, LOTTIE_SIZE_HOR * LOTTIE_SIZE_VER * sizeof(uint32_t), sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    ESP_GOTO_ON_FALSE(canvas_buf_888, ESP_ERR_NO_MEM, err, TAG, "Error malloc canvas buffer");

    canvas_buf_565 = heap_caps_aligned_calloc(64, LOTTIE_SIZE_HOR * LOTTIE_SIZE_VER * sizeof(uint16_t), sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    ESP_GOTO_ON_FALSE(canvas_buf_565, ESP_ERR_NO_MEM, err, TAG, "Error malloc canvas buffer");

    tvg_engine = tvg_engine_init(TVG_ENGINE_SW, 0);
    ESP_GOTO_ON_FALSE(tvg_engine == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    canvas = tvg_swcanvas_create();
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    tvg_res = tvg_swcanvas_set_target(canvas, canvas_buf_888, LOTTIE_SIZE_HOR, LOTTIE_SIZE_HOR, LOTTIE_SIZE_VER, TVG_COLORSPACE_ARGB8888);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    /* shape rect */
    Tvg_Paint *paint = tvg_shape_new();
    ESP_GOTO_ON_FALSE(paint, ESP_ERR_INVALID_STATE, err, TAG, "tvg_shape_new failed");

    tvg_res = tvg_shape_append_rect(paint, 0, 0, LOTTIE_SIZE_HOR, LOTTIE_SIZE_HOR, 0, 0);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_shape_append_rect failed");

    reac_color++;
    if (reac_color % 2) {
        tvg_res = tvg_shape_set_fill_color(paint, 255, 0, 0, 255);
    } else {
        tvg_res = tvg_shape_set_fill_color(paint, 0, 255, 0, 255);
    }
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_shape_set_fill_color failed");

    tvg_res = tvg_canvas_push(canvas, paint);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_push failed");

    tvg_res = tvg_canvas_draw(canvas);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_draw failed");

    tvg_res = tvg_canvas_sync(canvas);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_sync failed");

    argb888_to_rgb565_ppa(ppa_handle, canvas_buf_888, canvas_buf_565);
    esp_lcd_panel_draw_bitmap(lcd_panel->panel, 0, 0, LOTTIE_SIZE_HOR, LOTTIE_SIZE_VER, canvas_buf_565);

    /* tvg Lottie */
    animation = tvg_animation_new();
    ESP_GOTO_ON_FALSE(animation, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    Tvg_Paint *picture = tvg_animation_get_picture(animation);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    tvg_res = tvg_picture_load(picture, LOTTIE_FILENAME);
    ESP_GOTO_ON_FALSE(picture, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    tvg_res = tvg_picture_set_size(picture, LOTTIE_SIZE_HOR, LOTTIE_SIZE_VER);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    tvg_res = tvg_canvas_push(canvas, picture);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    float f_total;
    float f = 0;
    tvg_res = tvg_animation_get_total_frame(animation, &f_total);
    ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");
    ESP_GOTO_ON_FALSE((f_total != 0.0f), ESP_ERR_INVALID_STATE, err, TAG, "tvg_engine_init failed");

    uint32_t time_busy = 0;
    uint32_t anim_start = play_tick_get();

    while (f < f_total) {
        uint32_t frame_start = play_tick_get();

        tvg_res = tvg_animation_get_frame(animation, &f);
        f++;
        ESP_LOGI(TAG, "set %f / %f", f, f_total);
        tvg_res = tvg_animation_set_frame(animation, f);
        ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_animation_set_frame failed");

        tvg_res = tvg_canvas_update(canvas);
        ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_update failed");

        tvg_res = tvg_canvas_draw(canvas);
        ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_draw failed");

        tvg_res = tvg_canvas_sync(canvas);
        ESP_GOTO_ON_FALSE(tvg_res == TVG_RESULT_SUCCESS, ESP_ERR_INVALID_STATE, err, TAG, "tvg_canvas_sync failed");

        time_busy += play_tick_elaps(frame_start);

        argb888_to_rgb565_ppa(ppa_handle, canvas_buf_888, canvas_buf_565);
        esp_lcd_panel_draw_bitmap(lcd_panel->panel, 0, 0, LOTTIE_SIZE_HOR, LOTTIE_SIZE_VER, canvas_buf_565);

        uint32_t elaps_frame = play_tick_elaps(frame_start);
        if (elaps_frame < (1000 / EXPECTED_FPS)) {
            vTaskDelay(pdMS_TO_TICKS((1000 / EXPECTED_FPS) - elaps_frame));
        }
    }
    uint32_t elaps_anim = play_tick_elaps(anim_start);
    ESP_LOGI(TAG, "CPU:%" PRIu32 "%%, FPS:%d/%d", (time_busy * 100 / elaps_anim), (int)(1000 * f_total / elaps_anim), EXPECTED_FPS);

err:
    if (animation) {
        tvg_animation_del(animation);
    }
    if (canvas) {
        tvg_canvas_destroy(canvas);
    }
    if (TVG_RESULT_SUCCESS == tvg_engine) {
        tvg_engine_term(TVG_ENGINE_SW);
    }
    if (play_timer) {
        play_tick_del(play_timer);
    }

    if (canvas_buf_888) {
        free(canvas_buf_888);
    }
    if (canvas_buf_565) {
        free(canvas_buf_565);
    }

    return ret;
}

static esp_err_t argb888_to_rgb565_ppa(ppa_client_handle_t ppa_handle, const uint32_t *in, uint16_t *out)
{
    ppa_srm_oper_config_t oper_config = {
        .in.buffer = in,
        .in.pic_w = LOTTIE_SIZE_HOR,
        .in.pic_h = LOTTIE_SIZE_VER,
        .in.block_w = LOTTIE_SIZE_HOR,
        .in.block_h = LOTTIE_SIZE_VER,
        .in.block_offset_x = 0,
        .in.block_offset_y = 0,
        .in.srm_cm = PPA_SRM_COLOR_MODE_ARGB8888,

        .out.buffer = out,
        .out.buffer_size = LOTTIE_SIZE_HOR * LOTTIE_SIZE_VER * sizeof(uint16_t),
        .out.pic_w = LOTTIE_SIZE_HOR,
        .out.pic_h = LOTTIE_SIZE_VER,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = 1.0,
        .scale_y = 1.0,

        .rgb_swap = 0,
        .byte_swap = 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_handle, &oper_config));

    return ESP_OK;
}

static uint32_t play_tick_get(void)
{
    uint32_t result;
    do {
        tick_irq_flag = 1;
        result        = sys_time;
    } while (!tick_irq_flag);

    return result;
}

static void play_tick_inc(void *arg)
{
    uint32_t tick_period = (uint32_t)arg;
    tick_irq_flag = 0;
    sys_time += tick_period;
}

static void play_tick_new(esp_timer_handle_t *tm)
{
    esp_timer_handle_t timer;

    const uint32_t time_period = 2;
    // Create and start the event sources
    const esp_timer_create_args_t timer_args = {
        .callback = &play_tick_inc,
        .arg = (void *)time_period,
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, time_period * 1000));

    *tm = timer;
}

static void play_tick_del(esp_timer_handle_t tm)
{
    ESP_ERROR_CHECK(esp_timer_stop(tm));
    ESP_ERROR_CHECK(esp_timer_delete(tm));
}

static uint32_t play_tick_elaps(uint32_t prev_tick)
{
    uint32_t act_time = play_tick_get();

    /*If there is no overflow in sys_time simple subtract*/
    if (act_time >= prev_tick) {
        prev_tick = act_time - prev_tick;
    } else {
        prev_tick = UINT32_MAX - prev_tick + 1;
        prev_tick += act_time;
    }

    return prev_tick;
}
