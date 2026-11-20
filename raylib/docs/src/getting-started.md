# Getting Started

## Add the component to your project

```bash
idf.py add-dependency "espressif/raylib^6.2.0"
```

Or list it in `main/idf_component.yml`:

```yaml
dependencies:
  espressif/raylib: "^6.2.0"
```

Include both the upstream header and the component's own header:

```c
#include "raylib.h"
#include "esp_raylib.h"
```

## Register a display

Create and initialise your panel with whatever driver it needs — `esp_lcd` for most panels — then register a display config with [esp_raylib_add_display()](api.md#function-esp_raylib_add_display). Call it **before** `InitWindow()`.

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
```

See [Display Configuration](display.md) for the meaning of every field and the full callback contract.

## Draw your first frame

Display size comes from `InitWindow()` and nowhere else — it must match your panel resolution.

```c
InitWindow(320, 240, NULL);

while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Hello raylib!", 20, 100, 24, DARKGRAY);
    EndDrawing();
}
CloseWindow();
```

Give the rendering task a generous stack — 64 KiB is a reasonable start, since the renderer keeps texture and rasteriser state there. All raylib calls, `CloseWindow()` included, must come from the one rendering task.

> [!TIP]
> Frame pacing works as on a desktop: `SetTargetFPS(60)` spaces frames with real sleeps, which also keeps the watchdog fed. An uncapped loop still sleeps one tick per frame for the same reason.

## Troubleshooting striped or shifted colours

If the panel shows striped or shifted colours, the framebuffer format and [swap_bytes](api.md#struct-esp_raylib_display_config_t) disagree with what the panel takes. Check `CONFIG_RAYLIB_FRAMEBUFFER_FORMAT` and the [swap_bytes](api.md#struct-esp_raylib_display_config_t) flag first. See [Configuration Options](configuration.md).
