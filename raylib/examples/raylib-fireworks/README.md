# Raylib Fireworks Example

This example shows how to drive an SPI LCD with the [raylib](https://components.espressif.com/components/espressif/raylib) component's software renderer.

It draws a self-playing fireworks show on a 320x240 ST7789 panel: rockets rise from a generated city skyline and burst into particles under a night sky. There is no input and no asset file; every frame is immediate-mode raylib.

The application creates an `esp_lcd` ST7789 panel, registers it with `esp_raylib_add_display()`, and renders from a dedicated task. Frames are sent in 48-row bands so SPI DMA stays small; `panel_flush()` queues a transfer and returns, and `on_color_trans_done` calls `esp_raylib_flush_ready()` so the next band can be filled while this DMA is still on the wire.

## How to Use Example

### Hardware Required

* An ESP32-S3 development board with PSRAM (the default pinout matches an ESP32-S3-BOX style board)
* A USB cable for power supply and programming
* A 320x240 ST7789 SPI LCD

### Hardware Connection

The GPIO numbers used by this example can be changed in [raylib_example_main.c](main/raylib_example_main.c).

The backlight is driven active-high. Change `LCD_H_RES` and `LCD_V_RES` in the same file if the panel resolution is different.

### Configure the Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target esp32s3`.

The example enables PSRAM in `sdkconfig.defaults` so the software framebuffer can live in PSRAM. If the image is mirrored or upside down, adjust the `esp_lcd_panel_swap_xy()` / `esp_lcd_panel_mirror()` calls in `display_init()`, or change the `mirror` flags (`ESP_RAYLIB_MIRROR_X` / `ESP_RAYLIB_MIRROR_Y`) in the display config. Zero follows OpenGL; this example sets `ESP_RAYLIB_MIRROR_Y` because the ST7789 scans row 0 as the panel top.

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

A fireworks show should appear on the LCD. The console reports the software renderer coming up, then stays quiet (trace logging is off):

```text
I (767) main_task: Calling app_main()
I (891) raylib.idf: display 320x240, software renderer, 16 bpp framebuffer
INFO: RLSW: Software renderer initialized successfully
I (979) main_task: Returned from app_main()
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/idf-extra-components/issues) on GitHub. We will get back to you soon.
