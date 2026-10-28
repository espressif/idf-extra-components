/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a display registered with esp_raylib_add_display().
 */
typedef struct esp_raylib_display_t *esp_raylib_display_handle_t;

/**
 * @brief How the screen color buffer is mirrored relative to OpenGL.
 *
 * Zero follows GL: NDC +X/+Y are right and up, so row 0 is the bottom of
 * the scene. OR ESP_RAYLIB_MIRROR_X and/or ESP_RAYLIB_MIRROR_Y for panels
 * that cannot flip in hardware. The rasteriser writes already-mirrored
 * pixels; bands copy in memory order. Applies only to the screen color
 * buffer — a RenderTexture (BeginTextureMode) keeps API orientation.
 */
typedef enum {
    ESP_RAYLIB_MIRROR_X = 1 << 0, /**< Column 0 is the right of the scene */
    ESP_RAYLIB_MIRROR_Y = 1 << 1, /**< Row 0 is the top of the scene (typical LCD scanout) */
} esp_raylib_mirror_t;

/**
 * @brief Hand one band of a finished frame, or the whole frame, to the panel.
 *
 * @param display  The display delivering the frame -- the handle returned by
 *                 esp_raylib_add_display(). It identifies which display is calling.
 * @param pixels  w*h pixels in CONFIG_RAYLIB_FRAMEBUFFER_FORMAT, i.e.
 *                w*h*bytes_per_pixel bytes (1, 2, 3 or 4 per the selected format).
 * @param x,y     Origin of this slice in the color buffer (y is the first row)
 * @param w,h     Rectangle size
 * @param user_data  Opaque context from esp_raylib_display_config_t, passed
 *                   back untouched on every call
 *
 * @note Start the transfer and return; do not wait inside the callback.
 *       Call esp_raylib_flush_ready() exactly once when the display has
 *       finished reading `pixels`. Up to two transfers can be in flight at
 *       once, so report completions in the order the transfers were started.
 *
 *       With chunk_rows > 0, `pixels` is one of two staging bands in internal
 *       RAM and stays valid only until that flush_ready. With chunk_rows == 0,
 *       `pixels` is the color buffer itself (the component's allocator puts
 *       large buffers in PSRAM when CONFIG_RAYLIB_USE_PSRAM is on) and stays
 *       owned by the panel until a later frame reports it done; the driver
 *       must not keep reading it after that ready.
 */
typedef void (*esp_raylib_flush_cb_t)(esp_raylib_display_handle_t display, const void *pixels,
                                      uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      void *user_data);

/**
 * @brief How a display's frames are delivered to its panel.
 *
 * chunk_rows > 0: frames are delivered once per horizontal band of that many
 * rows. The component stages each band into one of two internal buffers of
 * w * chunk_rows * bpp bytes of internal RAM. A band taller than the screen
 * is cut down to the screen height.
 *
 * chunk_rows == 0: no staging buffers. flush_cb receives the whole color
 * buffer in one call (x=0, y=0, w, h). swap_bytes is ignored; the pixels are
 * native-endian in CONFIG_RAYLIB_FRAMEBUFFER_FORMAT.
 *
 * Both modes overlap the next write with the transfer still in flight: after
 * flush_cb returns, raylib waits for one flush_ready (InitWindow() posts the
 * first one), then fills the other band or draws the other color buffer.
 */
typedef struct {
    uint16_t chunk_rows;            /**< Rows per band, or 0 to hand the full color buffer to flush_cb */
    bool swap_bytes;                /**< Swap the two bytes of each pixel; RGB565 only; needs an even framebuffer width; ignored when chunk_rows is 0 */
    esp_raylib_mirror_t mirror;     /**< Screen color-buffer X/Y mirror; 0 = OpenGL; RenderTexture is not flipped */
    esp_raylib_flush_cb_t flush_cb; /**< Receives one band, or the whole frame; required */
    void *user_data;                /**< Opaque context, passed back to flush_cb untouched */
} esp_raylib_display_config_t;

/**
 * @brief Register a display for raylib to render to, and return its handle.
 *
 * Call before InitWindow(). The component never sees the LCD panel driver:
 * create and initialise it yourself and let flush_cb reach it through user_data.
 * Only one display is currently supported: a second call returns NULL.
 *
 * The registration lasts for the whole process: CloseWindow() tears down the
 * render and present machinery but leaves it in place, so InitWindow() can be
 * called again without repeating add_display(). user_data must outlive the
 * registration.
 *
 * Conventions the component does not defend at runtime:
 *  - InitWindow(w, h) must match the panel resolution: every band covers
 *    (0, y, w, y+h) in panel coordinates.
 *  - All raylib calls, CloseWindow() included, come from the one rendering
 *    task (raylib's own threading model).
 *
 * @param config Raylib platform display driver configuration
 * @return An opaque display handle, or NULL on a bad config or when a display has already been added.
 */
esp_raylib_display_handle_t esp_raylib_add_display(const esp_raylib_display_config_t *config);

/**
 * @brief Notify raylib that one flush_cb() transfer has finished.
 *
 * Call exactly once for every flush_cb, when the display is done reading
 * that call's `pixels`. Raylib waits for this before starting the next band
 * copy, or before recycling a color buffer.
 *
 * Where to call it:
 *  - SPI band DMA: from on_color_trans_done (this function is ISR-safe).
 *  - A synchronous copy inside flush_cb: call it before returning.
 *  - A full-frame (chunk_rows == 0) panel that swaps its own frame buffers:
 *    from the event that says the previous frame buffer is no longer being
 *    scanned (e.g. on_frame_buf_complete), once per flush_cb. The event must
 *    be edge-triggered per flush; a periodic VSYNC does not express "this
 *    buffer is free" and must not be used here.
 *
 * The callback and every object it touches live on the driver side: if the
 * LCD ISR is IRAM-safe, mark the callback IRAM_ATTR and allocate its user context from
 * internal RAM. esp_raylib_flush_ready() itself is IRAM_ATTR and only touches
 * internal-RAM state.
 *
 * Calls before InitWindow() and after CloseWindow() are ignored, so a
 * panel_init transfer or a late DMA cannot credit the next session.
 * Missing a call during a window deadlocks the present task.
 *
 * @param display The handle from esp_raylib_add_display()
 */
void esp_raylib_flush_ready(esp_raylib_display_handle_t display);

#ifdef __cplusplus
}
#endif
