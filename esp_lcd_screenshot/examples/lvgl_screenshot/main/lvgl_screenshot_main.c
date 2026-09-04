/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Render a static LVGL screen to a virtual esp_lcd panel (no LCD hardware
 * attached), save the captured framebuffer as a PNG file to LittleFS, and
 * dump the raw framebuffer over the serial console as base64.
 */

#include <stdio.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "lvgl.h"

#include "esp_lcd_screenshot.h"

#define EXAMPLE_LCD_H_RES       240
#define EXAMPLE_LCD_V_RES       240
#define EXAMPLE_DRAW_BUF_LINES  40

#define EXAMPLE_FS_MOUNT_POINT  "/littlefs"
#define EXAMPLE_PNG_PATH        EXAMPLE_FS_MOUNT_POINT "/screenshot.png"

static const char *TAG = "example";

/* Mount the LittleFS partition used to store the generated PNG file. */
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
    esp_littlefs_info(conf.partition_label, &total, &used);
    ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
    return ESP_OK;
}

/*
 * LVGL calls this function whenever a part of the screen is ready to be
 * displayed. The screenshot panel implements the regular esp_lcd panel API,
 * so the LVGL draw buffer can be passed to it just like it would be passed to
 * a physical LCD panel.
 */
static void example_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    /* The panel handle is stored in LVGL's user-data field below. */
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(display);

    /* LVGL's area uses inclusive x2/y2 coordinates, while esp_lcd uses an
     * exclusive right/bottom coordinate. Therefore, add one to x2 and y2. */
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    /* Tell LVGL that it may reuse the draw buffer for the next render. */
    lv_display_flush_ready(display);
}

/* A static screen: only the widget values matter, nothing depends on time,
 * so the rendered frame (and the test golden image) is deterministic. */
static void example_create_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x103a5c), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "esp_lcd_screenshot");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 200, 90);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 40);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_series_t *series = lv_chart_add_series(chart, lv_color_hex(0x5ce08a), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 10; i++) {
        lv_chart_set_next_value(chart, series, 20 + (i * i) % 60);
    }
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 55);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Press me");
    lv_obj_center(btn_label);

}

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize LittleFS");
    ESP_ERROR_CHECK(example_init_fs());

    ESP_LOGI(TAG, "Install virtual screenshot LCD panel driver");
    /*
     * The screenshot panel needs the display resolution and the byte layout
     * of the pixels it receives. LVGL's RGB888 buffer is stored as B, G, R,
     * which corresponds to ESP_COLOR_FOURCC_BGR24 (the FourCC string is BGR3).
     *
     * Set real_panel to an existing esp_lcd_panel_handle_t when you want to
     * capture frames while also showing them on a physical LCD. The screenshot
     * panel then owns that handle: initialize and delete only the screenshot
     * panel, not the original pointer. NULL creates a completely virtual
     * panel, as used by this example.
     */
    esp_lcd_screenshot_config_t panel_config = {
        .width = EXAMPLE_LCD_H_RES,
        .height = EXAMPLE_LCD_V_RES,
        .color_format = ESP_COLOR_FOURCC_BGR24,
        .real_panel = NULL,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_screenshot_panel(&panel_config, &panel_handle));
    /* Use the normal esp_lcd lifecycle before sending draw_bitmap calls. */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Register display driver to LVGL");
    /* Create an LVGL display with the same resolution as the panel. */
    lv_display_t *display = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    /* This must match panel_config.color_format and the draw buffer layout. */
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    /* Make the panel available to the flush callback via LVGL user data. */
    lv_display_set_user_data(display, panel_handle);
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    /* LVGL renders into this partial draw buffer. Each completed area is sent
     * to the screenshot panel by example_lvgl_flush_cb(), and the panel keeps
     * a full copy of the latest frame internally. */
    static uint8_t draw_buf[EXAMPLE_LCD_H_RES * EXAMPLE_DRAW_BUF_LINES * sizeof(lv_color_t)];
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Create a static demo UI and render it once");
    example_create_ui();
    /* Force an immediate refresh instead of waiting for an LVGL timer tick. */
    lv_refr_now(display);

    /* The PNG is written to the LittleFS mount initialized above. */
    ESP_LOGI(TAG, "Save the captured frame as a PNG file");
    ESP_ERROR_CHECK(esp_lcd_screenshot_save_png(panel_handle, EXAMPLE_PNG_PATH));
    struct stat st = {0};
    ESP_ERROR_CHECK(stat(EXAMPLE_PNG_PATH, &st) == 0 ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "PNG file size: %ld bytes", (long)st.st_size);

    ESP_LOGI(TAG, "Dump the captured frame over the serial console");
    /*
     * The output is delimited by FRAMEBUFFER_BEGIN/END markers. A host-side
     * script can collect the base64 payload and reconstruct the raw image.
     */
    ESP_ERROR_CHECK(esp_lcd_screenshot_dump_base64(panel_handle, stdout));

    ESP_LOGI(TAG, "LVGL screenshot example done.");
}
