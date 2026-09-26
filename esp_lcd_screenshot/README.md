# Capture Every Pixel: Virtual LCD Panel for ESP-IDF GUI

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_screenshot/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_screenshot)

`esp_lcd_screenshot` is a virtual [`esp_lcd`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html) panel driver for ESP-IDF. It captures the pixel data submitted by a GUI framework—such as LVGL—so you can inspect or compare rendered screens without connecting an LCD, taking camera photos, or relying on visual inspection.

The component supports two modes:

- **Virtual panel:** capture GUI output entirely in memory, even when the board has no display attached.
- **Decorator panel:** wrap an existing LCD panel. Frames are captured while the real display keeps working. The screenshot panel takes ownership of the wrapped handle.

Captured frames can be:

- saved as PNG files on LittleFS, an SD card, or another mounted filesystem; or
- dumped as base64 over a serial stream and reconstructed by a host-side test, making it suitable for automated golden-image tests.

## Installation

Add the component to your project's `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_lcd_screenshot: "^0.1.0"
```

Then run `idf.py reconfigure` or build the project. The component's PNG dependency is private and is resolved automatically by the IDF component manager.

> The component requires ESP-IDF `>= 6.0.0`.

## Quick start

Create the screenshot panel with the same resolution and pixel format as the framebuffer produced by your GUI library, initialize it, and pass the resulting panel handle to the library's display driver:

```c
#include "esp_lcd_screenshot.h"

esp_lcd_screenshot_config_t config = {
    .width = 240,
    .height = 240,
    .color_format = ESP_COLOR_FOURCC_BGR24, // LVGL RGB888 in memory
    .real_panel = NULL,                     // set to a real panel to capture and display
};

esp_lcd_panel_handle_t panel = NULL;
ESP_ERROR_CHECK(esp_lcd_new_screenshot_panel(&config, &panel));
ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

// Configure your GUI library to render/flush to `panel`.

// Save the most recently captured frame as PNG:
ESP_ERROR_CHECK(esp_lcd_screenshot_save_png(panel, "/littlefs/screenshot.png"));

// Or stream the raw framebuffer as base64 for a host-side test:
ESP_ERROR_CHECK(esp_lcd_screenshot_dump_base64(panel, stdout));
```

When wrapping a real panel, set `real_panel` **before** initializing it, then use only the screenshot handle. Do not call `esp_lcd_panel_del()` on the original pointer: deleting the screenshot panel also destroys the wrapped panel.

Match `width` and `height` to the GUI resolution. The capture is the pixel data the GUI submitted, not the physical LCD after rotation or color inversion.

Call `esp_lcd_screenshot_save_png()` or `esp_lcd_screenshot_dump_base64()` only after the GUI has finished the frame you want (for example after `lv_refr_now()`). The driver does not pause drawing, so that the real display keeps running.

For a complete working example with LVGL, LittleFS, serial capture, and a pytest golden-image test, see [`examples/lvgl_screenshot`](examples/lvgl_screenshot).

## Color formats

`color_format` uses ESP-IDF's `esp_color_fourcc_t`. Each `ESP_COLOR_FOURCC_xxx` macro is an integer representation of four ASCII characters, declared with `ESP_COLOR_FOURCC(a, b, c, d)`. The characters are also printed in the `FRAMEBUFFER_BEGIN` header produced by `esp_lcd_screenshot_dump_base64()`.

| Macro | Definition (Four Character Code) | Pixel data in memory |
| --- | --- | --- |
| `ESP_COLOR_FOURCC_RGB16` | `ESP_COLOR_FOURCC('R', 'G', 'B', 'L')` → `RGBL` | RGB565, little-endian (`uint16_t`) |
| `ESP_COLOR_FOURCC_RGB16_BE` | `ESP_COLOR_FOURCC('R', 'G', 'B', 'E')` → `RGBE` | RGB565, big-endian |
| `ESP_COLOR_FOURCC_RGB24` | `ESP_COLOR_FOURCC('R', 'G', 'B', '3')` → `RGB3` | 24-bit RGB888, bytes in R, G, B order |
| `ESP_COLOR_FOURCC_BGR24` | `ESP_COLOR_FOURCC('B', 'G', 'R', '3')` → `BGR3` | 24-bit BGR888, bytes in B, G, R order; commonly used by LVGL RGB888 |
| `ESP_COLOR_FOURCC_BGRA32` | `ESP_COLOR_FOURCC('B', 'A', '2', '4')` → `BA24` | 32-bit BGRA8888, bytes in B, G, R, A order |

Choose the format that matches the byte layout produced by your GUI library. For example, `ESP_COLOR_FOURCC_BGR24` corresponds to the integer value of `ESP_COLOR_FOURCC('B', 'G', 'R', '3')`, not to the text string stored in memory.

## Example output

`esp_lcd_screenshot_dump_base64()` writes a small framing protocol to the selected `FILE` stream:

```text
FRAMEBUFFER_BEGIN 240 240 BGR3
FB_BASE64 <base64-encoded framebuffer bytes>
FRAMEBUFFER_END
```

A host-side test can use the dimensions and Four Character Code in the first line to decode the payload with the correct bytes-per-pixel and color order.

## License

This component is released under the [Apache License 2.0](LICENSE).
