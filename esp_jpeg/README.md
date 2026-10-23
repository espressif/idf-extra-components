# Tiny JPEG Decoder

[![Component Registry](https://components.espressif.com/components/espressif/esp_jpeg/badge.svg)](https://components.espressif.com/components/espressif/esp_jpeg)
![maintenance-status](https://img.shields.io/badge/maintenance-actively--developed-brightgreen.svg)

`esp_jpeg` is a JPEG image decoder for ESP-IDF. Applications use `jpeg_decoder.h` (`esp_jpeg_decode()`, `esp_jpeg_get_image_info()`). The backend is not part of the public API and may change.

The current backend is [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html). Some ESP chips also provide a JPEG decoder in ROM; when that is available, this component uses it by default (`CONFIG_JD_USE_ROM=y`). Disable the option in menuconfig to compile the decoder from this component instead. The ROM decoder has a fixed compile-time configuration and does not include later workarounds such as default Huffman tables or the non-conforming `0xFFFF` marker.

## Features

**Compile-time configuration** (applied only when `CONFIG_JD_USE_ROM` is disabled):
- Stream input buffer size (default: 512 bytes)
- Output pixel format (default: RGB888; options: RGB888 / RGB565)
- Output descaling (default: enabled)
- Table-based saturation arithmetic (default: enabled)
- Default Huffman tables for camera frames that omit them (default: disabled, to save flash)
- Three optimization levels (default: 32-bit MCUs):
  - 8/16-bit MCUs
  - 32-bit MCUs
  - Table-based Huffman decoding

**Runtime configuration:**
- Pixel format: RGB888 or RGB565
- Scaling: 1/1, 1/2, 1/4, or 1/8
- Swap the first and last color bytes
- Optional user-provided working buffer (`advanced.working_buffer`)
- `esp_jpeg_get_image_info()` to read width, height, and output size without decoding

## ROM decoder

`CONFIG_JD_USE_ROM` is only offered when the target ROM provides a JPEG decoder (`ESP_ROM_HAS_JPEG_DECODE`). If that option appears in menuconfig, this chip has a ROM decoder and it is used by default. Disable it to compile the decoder from this component. On chips without the option, only the component sources are used.

### Fixed ROM compile-time settings
- Stream input buffer: 512 bytes
- Output pixel format: RGB888
- Output descaling: enabled
- Saturation table: enabled
- Optimization level: Basic (`JD_FASTDECODE = 0`)

### When to disable ROM
- You need any compile-time option above (output format, scale, Huffman, `JD_FASTDECODE`, …)
- You need to decode JPEGs that have no Huffman tables
- You need the non-conforming `0xFFFF` marker workaround

Using ROM saves about 5 KB of flash with the same settings. The source decoder can be faster when `JD_FASTDECODE` is 1 or 2; see the table below.

## Speed comparison

Example decoding times for one JPEG image:
* Image size: 320 x 180 px
* Output format: RGB565
* CPU: ESP32-S3 @ 240 MHz
* SPI mode: DIO
* Internal RAM
* Average of 1000 runs

| ROM used | JD_SZBUF | JD_FORMAT | JD_USE_SCALE | JD_TBLCLIP | JD_FASTDECODE | RAM buffer | Flash size | Approx. time |
| :------: | :------: | :-------: | :----------: | :--------: | :-----------: | :--------: | :--------: | :----------: |
|   YES    |    512   |   RGB888  |      1       |      1     |       0       |    3.1 kB  |    0 kB    |     52 ms    |
|   NO     |    512   |   RGB888  |      1       |      1     |       0       |    3.1 kB  |    5 kB    |     50 ms    |
|   NO     |    512   |   RGB888  |      1       |      0     |       0       |    3.1 kB  |    4 kB    |     68 ms    |
|   NO     |    512   |   RGB888  |      1       |      1     |       1       |    3.1 kB  |    5 kB    |     50 ms    |
|   NO     |    512   |   RGB888  |      1       |      0     |       1       |    3.1 kB  |    4 kB    |     62 ms    |
|   NO     |    512   |   RGB888  |      1       |      1     |       2       |   65.5 kB  |   5.5 kB   |     46 ms    |
|   NO     |    512   |   RGB888  |      1       |      0     |       2       |   65.5 kB  |   4.5 kB   |     59 ms    |
|   NO     |    512   |   RGB565  |      1       |      1     |       0       |    5 kB    |    5 kB    |     60 ms    |
|   NO     |    512   |   RGB565  |      1       |      1     |       1       |    5 kB    |    5 kB    |     59 ms    |
|   NO     |    512   |   RGB565  |      1       |      1     |       2       |   65.5 kB  |   5.5 kB   |     56 ms    |

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
Add this component with:

```
idf.py add-dependency "espressif/esp_jpeg^1.3.2"
```

Alternatively, create an `idf_component.yml`. See [ESP-IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

`esp_jpeg_decode()` is **blocking**. A complete LCD example is in [examples/get_started](examples/get_started).

```c
#include "jpeg_decoder.h"

esp_jpeg_image_cfg_t jpeg_cfg = {
    .indata = (uint8_t *)jpeg_img_buf,
    .indata_size = jpeg_img_buf_size,
    .outbuf = out_img_buf,
    .outbuf_size = out_img_buf_size,
    .out_format = JPEG_IMAGE_FORMAT_RGB565,
    .out_scale = JPEG_IMAGE_SCALE_0,
    .flags = {
        .swap_color_bytes = 1,
    }
};
esp_jpeg_image_output_t outimg;
esp_err_t err = esp_jpeg_decode(&jpeg_cfg, &outimg);
```

To size the output buffer first, call `esp_jpeg_get_image_info()` with the same `indata` / `out_format` / `out_scale`. It fills `outimg.width`, `outimg.height`, and `outimg.output_len` without writing pixels.
