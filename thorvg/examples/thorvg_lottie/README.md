# ThorVG Lottie example

This example combines [ThorVG](https://github.com/thorvg/thorvg), LittleFS, and `esp_lcd` to render a Lottie animation on an LCD. The default configuration targets an SH8601 panel connected through QSPI, but the application is intended to be adapted to other ESP chips and displays.

The example stores the animation in LittleFS, renders frames into an ARGB8888 buffer, converts them to RGB565, and sends them to the panel:

```text
Lottie JSON → LittleFS → ThorVG → ARGB8888 → RGB565 → LCD
```

## Requirements

- ESP-IDF version compatible with [`main/idf_component.yml`](main/idf_component.yml)
- An ESP chip with enough PSRAM for the render and display buffers
- An LCD supported by ESP-IDF's `esp_lcd` drivers, or a corresponding panel driver adapted in the example
- The wiring and interface required by the selected panel driver

The default source configuration uses an SH8601 panel in QSPI mode. The pin assignments, panel initialization commands, display dimensions, and SPI settings are defined near the beginning of [`main/thorvg_example_main.c`](main/thorvg_example_main.c).

## Build and run

From this directory:

```bash
idf.py set-target <target>
idf.py build
idf.py -p <PORT> flash monitor
```

For example, `<target>` can be `esp32s3` when using the default ESP32-S3 configuration. Select a target supported by the ESP-IDF version in use. If the target or board differs from the provided defaults, review the PSRAM, flash, partition, and LCD settings with `idf.py menuconfig` before building.

## Files and customization

- [`main/thorvg_example_main.c`](main/thorvg_example_main.c) focuses on the ThorVG usage and the rendering pipeline: Lottie loading, frame timing, rendering, color conversion, and LCD output.
- [`main/example_lcd.c`](main/example_lcd.c) and [`main/example_lcd.h`](main/example_lcd.h) contain the default SH8601/QSPI setup and expose the small display interface used by the rendering code. Adapt these files when changing the panel.
- [`lottie_files/emoji-animation.json`](lottie_files/emoji-animation.json) is the animation included in the LittleFS image. To try another animation, replace or add a JSON file and update the filename and dimensions in the source as needed.
- [`main/CMakeLists.txt`](main/CMakeLists.txt) packages `lottie_files` into the LittleFS partition during the build.
- [`partitions.csv`](partitions.csv) defines the application and storage partitions. Check that the storage partition can hold the animation files you add.

When adapting the example to another panel, update the panel component and the corresponding initialization code, interface settings, pin mapping, color format, and frame dimensions together. The display driver may also impose requirements on DMA-capable memory and transfer size.

## Memory considerations

Two frame buffers are allocated from PSRAM. At the default frame size, the ARGB8888 render buffer uses four bytes per pixel and the RGB565 display buffer uses two bytes per pixel; ThorVG, the application task, and the display driver require additional memory. Increasing the frame dimensions or using a larger animation can therefore require more PSRAM and a suitable partition layout.

## Further reading

- [ThorVG native API documentation](https://www.thorvg.org/native-apis)
- [ESP-IDF LCD API guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html)
- [ThorVG component](../../README.md)
