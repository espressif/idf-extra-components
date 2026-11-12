# Raylib, a simple 2D game programming library, running with software renderer

[![Component Registry](https://components.espressif.com/components/espressif/raylib/badge.svg)](https://components.espressif.com/components/espressif/raylib)

[Raylib](https://www.raylib.com/) is a small library for 2D graphics built around a plain C API (`InitWindow()`, `BeginDrawing()`, `DrawText()`, `EndDrawing()`). This component packages raylib for ESP-IDF and runs it on raylib's **software renderer**, so it works on any ESP chip — no GPU required. Your code renders into a framebuffer in memory; the component delivers finished frames to your display panel through one callback you write.

The 2D drawing API is the upstream one (`InitWindow()`, `BeginDrawing()`, `DrawText()`, `EndDrawing()`). Audio, 3D models, gestures and window/input devices are not adapted yet.

## What you get

- The upstream 2D drawing API, built from upstream's own CMake files
- The software rasteriser (rlsw): 2D shapes, textures and text
- A framebuffer pixel format chosen in menuconfig: RGB565 for most panels, RGBA8888, RGB888 or RGB332
- Panel delivery through one flush callback — DMA of one buffer overlaps drawing or copying the next
- Large buffers in PSRAM (optional) and a loader menu that keeps flash cost under control

## Add it to your project

```bash
idf.py add-dependency "espressif/raylib^6.2.0"
```

Or list it in `main/idf_component.yml`:

```yaml
dependencies:
  espressif/raylib: "^6.2.0"
```

Then include the upstream header plus the component's own:

```c
#include "raylib.h"
#include "esp_raylib.h"
```

## The component's API

The component adds three things on top of the upstream API, all declared in `esp_raylib.h`:

| Symbol | Purpose |
| --- | --- |
| `esp_raylib_add_display()` | Registers the panel that frames are delivered to, and returns a display handle. Call once, before `InitWindow()`. |
| `esp_raylib_flush_cb_t` | Your delivery callback. Receives one horizontal band of a finished frame, or the whole frame when `chunk_rows` is 0 — pixels in the configured framebuffer format, covering `(0, y, w, y+h)` in panel coordinates. |
| `esp_raylib_flush_ready()` | Call once per `flush_cb` when the display has finished reading those pixels (DMA done, or a sync copy). ISR-safe. |

`esp_raylib_display_config_t` configures the display:

| Field | Meaning |
| --- | --- |
| `chunk_rows` | Rows per band, or 0 to hand the software color buffer to `flush_cb` in one call. Non-zero: the component stages every band into one of two internal buffers of `w × chunk_rows × bpp` bytes of internal RAM. |
| `swap_bytes` | Swaps the two bytes of each pixel. RGB565 only; needs an even framebuffer width, or `InitWindow()` fails. |
| `mirror` | Screen color-buffer X/Y mirror, OR'd flags. Zero follows OpenGL (row 0 is the bottom of the scene). `ESP_RAYLIB_MIRROR_Y` puts row 0 at the top (typical LCD scanout); `ESP_RAYLIB_MIRROR_X` puts column 0 at the right. RenderTexture is not flipped. |
| `flush_cb` | The delivery callback. Required. |
| `user_data` | Opaque context passed back untouched on every invocation. |

## Wire up a display

Create and initialize your panel with whatever driver it needs — `esp_lcd` for most panels — then register a display config and start rendering:

```c
static void panel_flush(esp_raylib_display_handle_t display, const void *pixels,
                        uint16_t x, uint16_t y, uint16_t w, uint16_t h, void *user_data)
{
    my_ctx_t *ctx = user_data;   /* your esp_lcd panel handle and friends */
    esp_lcd_panel_draw_bitmap(ctx->panel, x, y, x + w, y + h, pixels);
    /* Queue DMA and return. Call esp_raylib_flush_ready() once when this
     * transfer has finished: from the SPI trans-done ISR, or here if the
     * copy is already done. */
}

...
const esp_raylib_display_config_t display_cfg = {
    .chunk_rows = 48,      /* send 48 lines at a time, fits SPI DMA */
    .swap_bytes = true,    /* RGB565 out MSB first; zero would mean no swap */
    .mirror     = ESP_RAYLIB_MIRROR_Y,  /* typical LCD: row 0 is the panel top */
    .flush_cb   = panel_flush,
    .user_data  = ctx,
};
esp_raylib_add_display(&display_cfg);

InitWindow(320, 240, NULL);          /* display size comes from here, nowhere else */

while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Hello raylib!", 20, 100, 24, DARKGRAY);
    EndDrawing();
}
CloseWindow();
```

The callback contract:

- It is called once per band in band order, or once per frame with the full color buffer when `chunk_rows` is 0.
- Start the transfer and return **without waiting for it**. Call `esp_raylib_flush_ready()` exactly once when those pixels have been consumed — from the SPI transfer-done ISR, inline for a synchronous copy, or from the full-frame event that releases the previous buffer. The calling context is detected internally; there is no semaphore to create. Calls before `InitWindow()` and after `CloseWindow()` are ignored.
- For SPI panels, `trans_queue_depth` of at least 2 is the usual `esp_lcd` setting. The component overlaps the next band *copy* with the transfer still in flight; stock `esp_lcd` SPI still waits out the previous color DMA before the next window command.
- The registration lasts for the whole process: `CloseWindow()` tears down the rendering side but keeps it, so `InitWindow()` can be called again without repeating `add_display()`. `user_data` must outlive the registration.

Three more rules: `InitWindow(w, h)` must match the panel resolution (the config holds no size); all raylib calls — `CloseWindow()` included — come from the one rendering task; only one display is currently supported (a second `add_display()` returns `NULL`).

Give the rendering task a generous stack — 64 KiB is a reasonable start, since the renderer keeps texture and rasteriser state there.

Frame pacing works as on a desktop: `SetTargetFPS(60)` spaces frames with real sleeps, which also keeps the watchdog fed. An uncapped loop still sleeps one tick per frame for the same reason.

If the panel shows striped or shifted colours, the framebuffer format and `swap_bytes` disagree with what the panel takes — check `CONFIG_RAYLIB_FRAMEBUFFER_FORMAT` and the `swap_bytes` flag first.

## Configure

Run `idf.py menuconfig` and open **Component config → Raylib**:

| Option | Default | Effect |
| --- | --- | --- |
| `CONFIG_RAYLIB_FRAMEBUFFER_FORMAT` | RGB565 | Pixel format of the framebuffer and of the pixels your callback receives. 16 bpp is what panels take natively; RGBA8888 is raylib's own format and needs no conversion. |
| `CONFIG_RAYLIB_USE_PSRAM` | on when PSRAM is available | Place large buffers (framebuffer, textures) in PSRAM instead of internal RAM; smaller ones stay internal. |
| `CONFIG_RAYLIB_FILEFORMAT_*` | all off | Which asset loaders are linked in, independently combinable: QOI, PNG, BMP, TGA, JPG and GIF images, and TTF fonts. QOI is raylib's own format and the cheapest; PNG uses raylib's built-in decoder (no zlib needed); TGA, JPG and GIF come from stb_image. With all off, assets come from memory, for example data embedded in the binary. |

## Example

- [examples/raylib-fireworks](examples/raylib-fireworks) drives an ST7789 panel over SPI on an ESP32-S3 with a self-playing fireworks show over a night skyline.

## License

Upstream raylib (`raylib/`) is [zlib](LICENSE). ESP-IDF packaging, the platform port (`src/`, `include/`) are Apache-2.0.

## Upstream documentation

This component does not re-document raylib — for the 2D drawing API, read the official material:

- [raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) — every function on one page
