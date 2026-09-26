/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "esp_lcd_screenshot.h"
#include "esp_lcd_panel_interface.h"
#include "hal/color_hal.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "png.h"

static const char *TAG = "lcd_screenshot";

#define SCREENSHOT_BASE64_CHUNK_IN     3072
#define SCREENSHOT_BASE64_CHUNK_OUT    (((SCREENSHOT_BASE64_CHUNK_IN + 2) / 3) * 4)
#define SCREENSHOT_BASE64_BUFFER_SIZE  (SCREENSHOT_BASE64_CHUNK_OUT + 1)

/*
 * esp_lcd_panel_t must be the first member so the object can be exposed as a
 * normal esp_lcd panel handle. Use __containerof() when recovering this
 * private object from the public base pointer.
 */
typedef struct {
    esp_lcd_panel_t base;
    int width;
    int height;
    size_t bytes_per_pixel;
    esp_color_fourcc_t color_format;
    uint8_t *framebuffer;
    esp_lcd_panel_handle_t real_panel;
} screenshot_panel_t;

static esp_err_t screenshot_del(esp_lcd_panel_t *panel);

/*
 * Forward a panel operation to the wrapped panel.
 * Virtual mode (no inner handle) is a successful no-op so GUI init sequences
 * keep working. A wrapped panel that leaves the method NULL returns
 * ESP_ERR_NOT_SUPPORTED instead of calling a NULL function pointer.
 */
#define SCREENSHOT_FORWARD(panel, op, ...) do { \
        screenshot_panel_t *_drv = __containerof((panel), screenshot_panel_t, base); \
        if (!_drv->real_panel) { \
            return ESP_OK; \
        } \
        ESP_RETURN_ON_FALSE(_drv->real_panel->op, ESP_ERR_NOT_SUPPORTED, TAG, \
                            #op " is not supported by the wrapped panel"); \
        return _drv->real_panel->op(_drv->real_panel, ##__VA_ARGS__); \
    } while (0)

/* FourCC values are packed little-endian: the first character occupies the
 * least-significant byte. Keep the printable form in sync with the value
 * reported in the base64 stream header. */
static void screenshot_fourcc_str(esp_color_fourcc_t fourcc, char out[5])
{
    out[0] = (char)(fourcc & 0xff);
    out[1] = (char)((fourcc >> 8) & 0xff);
    out[2] = (char)((fourcc >> 16) & 0xff);
    out[3] = (char)((fourcc >> 24) & 0xff);
    out[4] = '\0';
}

/* Check that the given handle was created by esp_lcd_new_screenshot_panel() */
static esp_err_t screenshot_check_panel(esp_lcd_panel_handle_t panel, screenshot_panel_t **ret_drv)
{
    /* Checking the destructor identifies handles owned by this driver before
     * applying __containerof() to the opaque esp_lcd panel pointer. */
    ESP_RETURN_ON_FALSE(panel && panel->del == screenshot_del, ESP_ERR_INVALID_ARG, TAG, "invalid panel handle");
    *ret_drv = __containerof(panel, screenshot_panel_t, base);
    return ESP_OK;
}

static esp_err_t screenshot_del(esp_lcd_panel_t *panel)
{
    screenshot_panel_t *drv = __containerof(panel, screenshot_panel_t, base);
    esp_err_t ret = ESP_OK;

    if (drv->real_panel) {
        /* This decorator takes ownership of real_panel, so destroying the
         * screenshot panel also destroys the wrapped panel. */
        if (drv->real_panel->del) {
            ret = drv->real_panel->del(drv->real_panel);
        } else {
            ESP_LOGW(TAG, "del is not supported by the wrapped panel");
        }
    }
    free(drv->framebuffer);
    free(drv);
    return ret;
}

/* Operations without a framebuffer side effect: forward them to the real
 * panel if there is one, otherwise behave as a virtual panel which is
 * always initialized, always on, and has no geometry adjustments to make. */
static esp_err_t screenshot_forward_reset(esp_lcd_panel_t *panel)
{
    SCREENSHOT_FORWARD(panel, reset);
}

static esp_err_t screenshot_forward_init(esp_lcd_panel_t *panel)
{
    SCREENSHOT_FORWARD(panel, init);
}

static esp_err_t screenshot_forward_mirror(esp_lcd_panel_t *panel, bool x_axis, bool y_axis)
{
    SCREENSHOT_FORWARD(panel, mirror, x_axis, y_axis);
}

static esp_err_t screenshot_forward_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    SCREENSHOT_FORWARD(panel, swap_xy, swap_axes);
}

static esp_err_t screenshot_forward_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    SCREENSHOT_FORWARD(panel, set_gap, x_gap, y_gap);
}

static esp_err_t screenshot_forward_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    SCREENSHOT_FORWARD(panel, invert_color, invert_color_data);
}

static esp_err_t screenshot_forward_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    SCREENSHOT_FORWARD(panel, disp_on_off, on_off);
}

static esp_err_t screenshot_forward_disp_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    SCREENSHOT_FORWARD(panel, disp_sleep, sleep);
}

static esp_err_t screenshot_forward_set_brightness(esp_lcd_panel_t *panel, int brightness)
{
    SCREENSHOT_FORWARD(panel, set_brightness, brightness);
}

/* Copy a source crop into the screenshot framebuffer. The source crop and
 * target rectangle must have the same dimensions (no scaling). */
static esp_err_t screenshot_copy_bitmap(screenshot_panel_t *drv,
                                        int x_start, int y_start, int x_end, int y_end,
                                        const void *src_data, size_t src_x_size, size_t src_y_size,
                                        int src_x_start, int src_y_start, int src_x_end, int src_y_end)
{
    int64_t target_width = (int64_t)x_end - x_start;
    int64_t target_height = (int64_t)y_end - y_start;
    int64_t source_width = (int64_t)src_x_end - src_x_start;
    int64_t source_height = (int64_t)src_y_end - src_y_start;

    /* The public esp_lcd wrapper validates target/source ordering. The
     * remaining checks protect this driver's source-buffer calculations. */
    ESP_RETURN_ON_FALSE(src_data
                        && source_width == target_width && source_height == target_height
                        && src_x_start >= 0 && src_y_start >= 0
                        && (uint64_t)src_x_end <= src_x_size && (uint64_t)src_y_end <= src_y_size
                        && src_x_size <= SIZE_MAX / drv->bytes_per_pixel
                        && src_x_size > 0
                        && src_y_size <= SIZE_MAX / src_x_size,
                        ESP_ERR_INVALID_ARG, TAG, "invalid bitmap region");

    /* Clip the target to the screenshot framebuffer and shift the source crop
     * by exactly the same amount so source and destination stay aligned. */
    int clipped_x_start = MAX(x_start, 0);
    int clipped_y_start = MAX(y_start, 0);
    int clipped_x_end = MIN(x_end, drv->width);
    int clipped_y_end = MIN(y_end, drv->height);
    int64_t clipped_src_x_start = (int64_t)src_x_start + clipped_x_start - x_start;
    int64_t clipped_src_y_start = (int64_t)src_y_start + clipped_y_start - y_start;

    if (clipped_x_start < clipped_x_end && clipped_y_start < clipped_y_end) {
        const uint8_t *src = (const uint8_t *)src_data
                             + ((size_t)clipped_src_y_start * src_x_size + (size_t)clipped_src_x_start) * drv->bytes_per_pixel;
        uint8_t *dst = drv->framebuffer
                       + ((size_t)clipped_y_start * drv->width + clipped_x_start) * drv->bytes_per_pixel;
        size_t copy_bytes = (size_t)(clipped_x_end - clipped_x_start) * drv->bytes_per_pixel;
        size_t src_stride = src_x_size * drv->bytes_per_pixel;
        size_t dst_stride = (size_t)drv->width * drv->bytes_per_pixel;

        for (int y = clipped_y_start; y < clipped_y_end; y++) {
            memcpy(dst, src, copy_bytes);
            src += src_stride;
            dst += dst_stride;
        }
    }
    return ESP_OK;
}

static esp_err_t screenshot_draw_bitmap(esp_lcd_panel_t *panel,
                                        int x_start, int y_start,
                                        int x_end, int y_end,
                                        const void *color_data)
{
    screenshot_panel_t *drv = __containerof(panel, screenshot_panel_t, base);
    /* Packed source: the bitmap is exactly the destination window. */
    size_t src_x_size = (size_t)((int64_t)x_end - x_start);
    size_t src_y_size = (size_t)((int64_t)y_end - y_start);
    esp_err_t ret = screenshot_copy_bitmap(drv, x_start, y_start, x_end, y_end, color_data,
                                           src_x_size, src_y_size, 0, 0, (int)src_x_size, (int)src_y_size);
    if (ret != ESP_OK) {
        return ret;
    }
    /* Capture first, then forward the original request so wrapping a 1D-only
     * panel (e.g. ST7789) does not require draw_bitmap_2d. */
    SCREENSHOT_FORWARD(panel, draw_bitmap, x_start, y_start, x_end, y_end, color_data);
}

static esp_err_t screenshot_draw_bitmap_2d(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *src_data, size_t src_x_size, size_t src_y_size,
                                           int src_x_start, int src_y_start, int src_x_end, int src_y_end)
{
    screenshot_panel_t *drv = __containerof(panel, screenshot_panel_t, base);
    esp_err_t ret = screenshot_copy_bitmap(drv, x_start, y_start, x_end, y_end, src_data,
                                           src_x_size, src_y_size, src_x_start, src_y_start, src_x_end, src_y_end);
    if (ret != ESP_OK) {
        return ret;
    }
    SCREENSHOT_FORWARD(panel, draw_bitmap_2d, x_start, y_start, x_end, y_end, src_data,
                       src_x_size, src_y_size, src_x_start, src_y_start, src_x_end, src_y_end);
}

esp_err_t esp_lcd_new_screenshot_panel(const esp_lcd_screenshot_config_t *config,
                                       esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(config->width > 0 && config->height > 0, ESP_ERR_INVALID_ARG, TAG, "invalid panel size");

    uint32_t bits_per_pixel = color_hal_pixel_format_fourcc_get_bit_depth(config->color_format);
    ESP_RETURN_ON_FALSE(bits_per_pixel > 0 && bits_per_pixel % 8 == 0,
                        ESP_ERR_INVALID_ARG, TAG, "unsupported color format");
    size_t bytes_per_pixel = bits_per_pixel / 8;

    size_t width = (size_t)config->width;
    size_t height = (size_t)config->height;
    ESP_RETURN_ON_FALSE(width <= SIZE_MAX / height, ESP_ERR_INVALID_ARG, TAG, "panel size overflow");
    size_t pixel_count = width * height;
    ESP_RETURN_ON_FALSE(pixel_count <= SIZE_MAX / bytes_per_pixel,
                        ESP_ERR_INVALID_ARG, TAG, "framebuffer size overflow");

    screenshot_panel_t *drv = calloc(1, sizeof(screenshot_panel_t));
    ESP_RETURN_ON_FALSE(drv, ESP_ERR_NO_MEM, TAG, "no memory for panel");

    drv->width = config->width;
    drv->height = config->height;
    drv->bytes_per_pixel = bytes_per_pixel;
    drv->color_format = config->color_format;
    drv->real_panel = config->real_panel;

    size_t fb_size = pixel_count * drv->bytes_per_pixel;
    /* calloc initializes the screenshot to black until the first draw_bitmap
     * call, which also makes partially rendered frames well-defined. */
    drv->framebuffer = calloc(1, fb_size);
    ESP_GOTO_ON_FALSE(drv->framebuffer, ESP_ERR_NO_MEM, err_free_panel, TAG,
                      "no memory for framebuffer (%zu bytes)", fb_size);

    drv->base.del = screenshot_del;
    drv->base.reset = screenshot_forward_reset;
    drv->base.init = screenshot_forward_init;
    drv->base.draw_bitmap = screenshot_draw_bitmap;
    drv->base.draw_bitmap_2d = screenshot_draw_bitmap_2d;
    drv->base.mirror = screenshot_forward_mirror;
    drv->base.swap_xy = screenshot_forward_swap_xy;
    drv->base.set_gap = screenshot_forward_set_gap;
    drv->base.invert_color = screenshot_forward_invert_color;
    drv->base.disp_on_off = screenshot_forward_disp_on_off;
    drv->base.disp_sleep = screenshot_forward_disp_sleep;
    drv->base.set_brightness = screenshot_forward_set_brightness;

    *ret_panel = &drv->base;
    char fourcc_str[5];
    screenshot_fourcc_str(config->color_format, fourcc_str);
    ESP_LOGI(TAG, "Screenshot panel created (%dx%d, %s, %s)",
             config->width, config->height, fourcc_str,
             config->real_panel ? "wrapping real panel" : "virtual only");
    return ESP_OK;

err_free_panel:
    free(drv);
    return ret;
}

esp_err_t esp_lcd_screenshot_dump_base64(esp_lcd_panel_handle_t panel, FILE *stream)
{
    esp_err_t ret = ESP_OK;
    screenshot_panel_t *drv = NULL;
    ESP_RETURN_ON_ERROR(screenshot_check_panel(panel, &drv), TAG, "invalid panel handle");

    if (!stream) {
        stream = stdout;
    }

    size_t fb_size = (size_t)drv->width * drv->height * drv->bytes_per_pixel;

    /* Encode in fixed-size chunks. Each payload line carries the FB_BASE64
     * prefix so that a host parsing the serial output can tell it apart from
     * interleaved log lines. */
    const size_t chunk_in = SCREENSHOT_BASE64_CHUNK_IN;
    char *b64_line = malloc(SCREENSHOT_BASE64_BUFFER_SIZE);
    if (!b64_line) {
        return ESP_ERR_NO_MEM;
    }

    char fourcc_str[5];
    screenshot_fourcc_str(drv->color_format, fourcc_str);
    /* Lock the FILE stream for the complete framed message so other stdio
     * users cannot interleave output into the host-side protocol. */
    flockfile(stream);

    fprintf(stream, "FRAMEBUFFER_BEGIN %d %d %s\n", drv->width, drv->height, fourcc_str);

    for (size_t offset = 0; offset < fb_size; offset += chunk_in) {
        size_t remaining = fb_size - offset;
        size_t this_chunk = MIN(remaining, chunk_in);
        size_t b64_len = 0;
        int encode_ret = mbedtls_base64_encode((unsigned char *)b64_line, SCREENSHOT_BASE64_BUFFER_SIZE,
                                               &b64_len, drv->framebuffer + offset, this_chunk);
        ESP_GOTO_ON_FALSE(encode_ret == 0, ESP_FAIL, dump_cleanup, TAG,
                          "failed to encode framebuffer as base64 (%d)", encode_ret);
        b64_line[b64_len] = '\0';
        fputs("FB_BASE64 ", stream);
        fputs(b64_line, stream);
        fputc('\n', stream);
        /* Console TX busy-waits without yielding; a full dump can exceed the
         * task WDT timeout. Yield one tick so IDLE can feed the watchdog.
         * vTaskDelay(1): pdMS_TO_TICKS(1) can round to 0 at 100 Hz. */
        vTaskDelay(1);
    }

    fprintf(stream, "FRAMEBUFFER_END\n");
    ESP_GOTO_ON_FALSE(fflush(stream) != EOF, ESP_FAIL, dump_cleanup, TAG,
                      "failed to flush framebuffer output");

dump_cleanup:
    free(b64_line);
    funlockfile(stream);
    return ret;
}

static uint8_t rgb565_to_rgb888_r(uint16_t c)
{
    uint8_t r = (c >> 11) & 0x1f;
    return (uint8_t)((r << 3) | (r >> 2));
}

static uint8_t rgb565_to_rgb888_g(uint16_t c)
{
    uint8_t g = (c >> 5) & 0x3f;
    return (uint8_t)((g << 2) | (g >> 4));
}

static uint8_t rgb565_to_rgb888_b(uint16_t c)
{
    uint8_t b = c & 0x1f;
    return (uint8_t)((b << 3) | (b >> 2));
}

static bool screenshot_bgra_alpha_all_zero(const screenshot_panel_t *drv)
{
    const uint8_t *src = drv->framebuffer;
    size_t pixel_count = (size_t)drv->width * drv->height;
    for (size_t i = 0; i < pixel_count; i++) {
        if (src[i * 4 + 3] != 0) {
            return false;
        }
    }
    return true;
}

/* Fill one PNG scanline (RGB or RGBA, 8 bits per channel) from framebuffer row y. */
static void screenshot_fill_png_row(const screenshot_panel_t *drv, int y,
                                    png_bytep row, unsigned char channels)
{
    const uint8_t *fb = drv->framebuffer;
    const int width = drv->width;

    switch (drv->color_format) {
    case ESP_COLOR_FOURCC_RGB16:
    case ESP_COLOR_FOURCC_RGB16_BE: {
        bool big_endian = (drv->color_format == ESP_COLOR_FOURCC_RGB16_BE);
        const uint8_t *src = fb + (size_t)y * width * 2;
        for (int x = 0; x < width; x++) {
            uint16_t c = big_endian
                         ? ((uint16_t)src[x * 2] << 8) | src[x * 2 + 1]
                         : src[x * 2] | ((uint16_t)src[x * 2 + 1] << 8);
            row[x * 3 + 0] = rgb565_to_rgb888_r(c);
            row[x * 3 + 1] = rgb565_to_rgb888_g(c);
            row[x * 3 + 2] = rgb565_to_rgb888_b(c);
        }
        break;
    }
    case ESP_COLOR_FOURCC_RGB24:
        memcpy(row, fb + (size_t)y * width * 3, (size_t)width * 3);
        break;
    case ESP_COLOR_FOURCC_BGR24: {
        const uint8_t *src = fb + (size_t)y * width * 3;
        for (int x = 0; x < width; x++) {
            row[x * 3 + 0] = src[x * 3 + 2];
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        break;
    }
    case ESP_COLOR_FOURCC_BGRA32: {
        const uint8_t *src = fb + (size_t)y * width * 4;
        for (int x = 0; x < width; x++) {
            row[x * channels + 0] = src[x * 4 + 2];
            row[x * channels + 1] = src[x * 4 + 1];
            row[x * channels + 2] = src[x * 4 + 0];
            if (channels == 4) {
                row[x * 4 + 3] = src[x * 4 + 3];
            }
        }
        break;
    }
    default:
        memset(row, 0, (size_t)width * channels);
        break;
    }
}

esp_err_t esp_lcd_screenshot_save_png(esp_lcd_panel_handle_t panel, const char *filepath)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(filepath, ESP_ERR_INVALID_ARG, TAG, "filepath is NULL");
    screenshot_panel_t *drv = NULL;
    ESP_RETURN_ON_ERROR(screenshot_check_panel(panel, &drv), TAG, "invalid panel handle");

    /* BGRA32 can be used as XRGB8888 by some GUI pipelines. Treat an
     * all-zero alpha channel as opaque so the resulting PNG is useful rather
     * than fully transparent. */
    bool xrgb_as_opaque = false;
    unsigned char channels = 3;
    if (drv->color_format == ESP_COLOR_FOURCC_BGRA32) {
        xrgb_as_opaque = screenshot_bgra_alpha_all_zero(drv);
        channels = xrgb_as_opaque ? 3 : 4;
    }

    FILE *f = fopen(filepath, "wb");
    ESP_GOTO_ON_FALSE(f, ESP_FAIL, err, TAG, "failed to open '%s'", filepath);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    ESP_GOTO_ON_FALSE(png_ptr, ESP_ERR_NO_MEM, err_close_file, TAG,
                      "png_create_write_struct failed");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_bytep row = malloc((size_t)drv->width * channels);
    ESP_GOTO_ON_FALSE(info_ptr && row, ESP_ERR_NO_MEM, err_destroy_png, TAG,
                      "no memory for PNG encoder");

    /* libpng reports write errors through longjmp rather than return values;
     * release every resource here before propagating the failure. */
    if (setjmp(png_jmpbuf(png_ptr))) {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "failed to write PNG file '%s'", filepath);
        goto err_destroy_png;
    }

    png_init_io(png_ptr, f);
    png_set_IHDR(png_ptr, info_ptr, (png_uint_32)drv->width, (png_uint_32)drv->height, 8,
                 channels == 4 ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_sRGB(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < drv->height; y++) {
        screenshot_fill_png_row(drv, y, row, channels);
        png_write_row(png_ptr, row);
    }
    png_write_end(png_ptr, NULL);

    long written = ftell(f);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(f);

    ESP_LOGI(TAG, "Saved %dx%d PNG image (%ld bytes) to '%s'",
             drv->width, drv->height, written, filepath);
    return ESP_OK;

err_destroy_png:
    free(row);
    png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : NULL);
err_close_file:
    fclose(f);
    /* fopen(..., "wb") already truncated the path; do not leave a partial PNG. */
    if (remove(filepath) != 0) {
        ESP_LOGW(TAG, "failed to remove incomplete PNG '%s'", filepath);
    }
err:
    return ret;
}
