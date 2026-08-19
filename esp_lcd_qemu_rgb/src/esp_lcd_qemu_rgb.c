/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_qemu_rgb_struct.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "soc/syscon_reg.h"

/* "QEMU" as a 32-bit value, used to check whether the current application is running in
 * QEMU or on real hardware */
#define RGB_QEMU_ORIGIN     0x51454d55

static const char *TAG = "lcd_qemu.rgb";

static rgb_qemu_dev_t *s_rgb_dev = (void *) 0x21000000;
static uint32_t *s_rgb_framebuffer = (void *) 0x20000000;

/* Software handler for the RGB Qemu virtual panel */
typedef struct esp_rgb_qemu_t {
    esp_lcd_panel_t base;  // Base class of generic lcd panel
    int panel_id;          // LCD panel ID
    uint32_t width;
    uint32_t height;
} esp_rgb_qemu_t;

static_assert(offsetof(esp_rgb_qemu_t, base) == 0, "Base field must be the first");

static esp_err_t rgb_qemu_del(esp_lcd_panel_t *panel);
static esp_err_t rgb_qemu_reset(esp_lcd_panel_t *panel);
static esp_err_t rgb_qemu_init(esp_lcd_panel_t *panel);
static esp_err_t rgb_qemu_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t rgb_qemu_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t rgb_qemu_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t rgb_qemu_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t rgb_qemu_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t rgb_qemu_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_rgb_qemu(const esp_lcd_rgb_qemu_config_t *rgb_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    esp_rgb_qemu_t *rgb_panel = NULL;
    ESP_GOTO_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid parameter");

    /* Check if we are actually running on QEMU, read the special register allocated just before the
     * SYSCON date one. */
    const uint32_t origin = REG_READ(SYSCON_DATE_REG - 4);

    /* In case we are running in QEMU, this register contains "QEMU" as a 32-bit value */
    ESP_GOTO_ON_FALSE(origin == RGB_QEMU_ORIGIN, ESP_ERR_NOT_SUPPORTED, err, TAG, "qemu panel is not available on real hardware");

    rgb_panel = calloc(1, sizeof(esp_rgb_qemu_t));
    ESP_GOTO_ON_FALSE(rgb_panel, ESP_ERR_NO_MEM, err, TAG, "no mem for rgb qemu panel");

    /* Resize the window and setup bpp*/
    s_rgb_dev->size.height = rgb_config->height;
    s_rgb_dev->size.width = rgb_config->width;
    s_rgb_dev->bpp = rgb_config->bpp ? rgb_config->bpp : RGB_QEMU_BPP_32;
    /* If the configured size is bigger than authorized, the hardware will arrange it.
     * So, read back the configured size */
    rgb_panel->height = rgb_config->height;
    rgb_panel->width = rgb_config->width;

    /* Fill function table */
    rgb_panel->base.del = rgb_qemu_del;
    rgb_panel->base.reset = rgb_qemu_reset;
    rgb_panel->base.init = rgb_qemu_init;
    rgb_panel->base.draw_bitmap = rgb_qemu_draw_bitmap;
    rgb_panel->base.disp_on_off = rgb_qemu_disp_on_off;
    rgb_panel->base.invert_color = rgb_qemu_invert_color;
    rgb_panel->base.mirror = rgb_qemu_mirror;
    rgb_panel->base.swap_xy = rgb_qemu_swap_xy;
    rgb_panel->base.set_gap = rgb_qemu_set_gap;

    /* Return base class */
    *ret_panel = &(rgb_panel->base);
    ret = ESP_OK;

err:
    return ret;
}

esp_err_t esp_lcd_rgb_qemu_get_frame_buffer(esp_lcd_panel_handle_t panel, void **fb)
{
    if (fb) {
        *fb = (void *) s_rgb_framebuffer;
    }

    return ESP_OK;
}

esp_err_t esp_lcd_rgb_qemu_refresh(esp_lcd_panel_handle_t panel)
{
    esp_rgb_qemu_t *rgb_panel = (esp_rgb_qemu_t *) panel;
    return rgb_qemu_draw_bitmap(panel, 0, 0, rgb_panel->width, rgb_panel->height, s_rgb_framebuffer);
}

/*** PRIVATE FUNCTIONS ***/

static esp_err_t rgb_qemu_del(esp_lcd_panel_t *panel)
{
    free(panel);
    return ESP_OK;
}

static esp_err_t rgb_qemu_reset(esp_lcd_panel_t *panel)
{
    return ESP_OK;
}

static esp_err_t rgb_qemu_init(esp_lcd_panel_t *panel)
{
    return ESP_OK;
}

static esp_err_t rgb_qemu_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    esp_rgb_qemu_t *rgb_panel = (esp_rgb_qemu_t *) panel;
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    s_rgb_dev->update_from.x = x_start;
    s_rgb_dev->update_from.y = y_start;
    /* The rendering WON'T include end (x,y) coordinates  */
    s_rgb_dev->update_to.x = x_end;
    s_rgb_dev->update_to.y = y_end;
    s_rgb_dev->update_content = (void *) color_data;
    s_rgb_dev->update_st.ena = 1;
    /* Wait for the driver to finish updating the window to avoid screen tearing effect.
     * This issue is on the ESP32 QEMU target (making this loop necessary) but not on the ESP32-C3. */
    while (s_rgb_dev->update_st.ena == 1) {}

    (void) rgb_panel;
    return ESP_OK;
}

static esp_err_t rgb_qemu_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rgb_qemu_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rgb_qemu_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rgb_qemu_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rgb_qemu_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    return ESP_ERR_NOT_SUPPORTED;
}
