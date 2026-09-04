# Changelog

## 0.1.0

- Initial version
- Virtual esp_lcd panel which captures `draw_bitmap` / `draw_bitmap_2d` calls into an internal framebuffer (submitted pixels; rotation/gap/invert are not applied to the screenshot)
- Optional decorator mode: pass a real panel in `esp_lcd_screenshot_config_t::real_panel`; the screenshot panel takes ownership and forwards operations to it
- `esp_lcd_screenshot_config_t::color_format` takes an `esp_color_fourcc_t` (RGB16, RGB16_BE, RGB24, BGR24, BGRA32)
- `esp_lcd_screenshot_dump_base64()` streams the framebuffer as base64 over a `FILE*` (e.g. the serial console), yielding between lines so a long serial dump does not trip the task watchdog
- `esp_lcd_screenshot_save_png()` streams scanlines into a PNG file via libpng `png_write_row` (convert and compress together, no extra full-frame RGB buffer)
