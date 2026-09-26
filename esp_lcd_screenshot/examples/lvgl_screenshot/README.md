# LVGL Screenshot: Render, Capture, and Verify Without an LCD

This example demonstrates how to use the `esp_lcd_screenshot` component with LVGL. It renders a static UI to a virtual `esp_lcd` panel, captures the rendered pixels, saves the frame as a PNG file, and streams the same frame as base64 for automated image verification.

No LCD is connected to the board—the screenshot panel receives the same `draw_bitmap()` calls that a physical LCD panel would normally receive.

## What the example does

1. Mounts a LittleFS partition at `/littlefs`.
2. Creates a virtual 240 × 240 RGB888 screenshot panel using `ESP_COLOR_FOURCC_BGR24`.
3. Registers the panel as an LVGL display and renders a static screen containing a title, chart, and button.
4. Saves the latest captured frame as `/littlefs/screenshot.png`.
5. Dumps the raw framebuffer over the serial console as base64.
6. Uses a pytest script to decode the framebuffer and compare it with `golden_result.ppm`.

The screen is rendered once without an LVGL task or tick timer. Keeping the UI static makes the output deterministic and suitable for golden-image testing.

## Hardware requirements

- An ESP32-series development board with **PSRAM**
- A USB connection for flashing and serial output
- No LCD or other external hardware

This example is **not compatible with every ESP32 development board**. PSRAM is required because the example and its dependencies allocate framebuffer and image-processing memory externally. The example enables PSRAM through `CONFIG_SPIRAM=y` in [`sdkconfig.defaults`](sdkconfig.defaults). Make sure the selected board actually has PSRAM and that its flash/PSRAM configuration is supported by the chosen ESP-IDF target.

## Build and run

From this directory, select the target that matches your board, then build, flash, and open the serial monitor:

```bash
idf.py set-target esp32
idf.py build flash monitor
```

Replace `esp32` with the appropriate ESP-IDF target when necessary. Exit the monitor before running commands that need exclusive access to the serial port.

The serial output contains messages similar to:

```text
I (350) example: Install virtual screenshot LCD panel driver
I (360) lcd_screenshot: Screenshot panel created (240x240, BGR3, virtual only)
I (600) example: Save the captured frame as a PNG file
I (650) lcd_screenshot: Saved 240x240 PNG image (4321 bytes) to '/littlefs/screenshot.png'
I (660) example: PNG file size: 4321 bytes
I (670) example: Dump the captured frame over the serial console
FRAMEBUFFER_BEGIN 240 240 BGR3
FB_BASE64 3Hj9/+H...
FB_BASE64 ...
FRAMEBUFFER_END
I (1200) example: LVGL screenshot example done.
```

The framebuffer is RGB888 in B, G, R byte order (`BGR3`). The base64 payload is enclosed by `FRAMEBUFFER_BEGIN` and `FRAMEBUFFER_END` markers and contains `240 × 240 × 3` bytes.

## Copy the PNG from LittleFS

The example stores the PNG in the `storage` partition. To copy that partition from a connected board, first exit the serial monitor and run:

```bash
parttool.py --port PORT read_partition --partition-name=storage --output storage.bin
```

Replace `PORT` with the serial port used by your board, for example `/dev/ttyUSB0` or `COM3`.

`storage.bin` is a LittleFS image. Extract it on the host with [`littlefs-python`](https://pypi.org/project/littlefs-python/):

```bash
pip install littlefs-python
littlefs-python extract storage.bin out/ --block-size=4096
```

The captured image will be available at:

```text
out/screenshot.png
```

## Automated verification

The pytest script captures the serial framebuffer, decodes the base64 payload, converts it to an RGB888 PPM image, and compares it with [`golden_result.ppm`](golden_result.ppm) using a SHA-256 digest.

Run it from this directory:

```bash
pytest pytest_lvgl_screenshot.py --target esp32 --port PORT
```

Replace `PORT` with your board's serial port. The test requires a connected board with PSRAM and will flash/run the example as part of the pytest-embedded workflow.

The decoded image is also saved as `lvgl_screenshot_result.ppm` in the pytest-embedded log directory, typically below `/tmp/pytest-embedded/`, which is useful when inspecting a test failure.

### Capturing a frame in an application

The generic `esp_lcd` interface has no concept of a complete GUI frame: a frame may consist of multiple partial flushes. Call an export function only after the refresh you want is finished (for example after a synchronous `lv_refr_now()`, as in this example). The screenshot driver does not pause drawing, so do not export from another task while the GUI is still flushing that frame.
