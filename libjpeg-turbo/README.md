# libjpeg-turbo: the JPEG codec, packaged for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/libjpeg-turbo/badge.svg)](https://components.espressif.com/components/espressif/libjpeg-turbo)

This component is the ESP-IDF packaging of [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo), the well-known high-speed JPEG library. It is not a new API: include the upstream headers and call the classic **libjpeg C API**.

Use it to turn a camera framebuffer into a JPEG, or a JPEG file into RGB pixels you can push to a display. libjpeg-turbo is an ISO/IEC and ITU-T reference JPEG implementation. On ESP chips its SIMD backends are not available, so this port builds the portable C codec: still a full encoder and decoder, sized for embedded use.

## Features

- **Encode and decode** baseline and progressive JPEG through `jpeglib.h`.
- **In-memory I/O** with `jpeg_mem_src()` / `jpeg_mem_dest()` — no filesystem required, so camera snapshots and embedded assets work as-is.
- **Colorspace extensions** from libjpeg-turbo (RGB, RGBX, and related packed formats) in addition to the traditional JPEG color spaces.
- **Arithmetic coding** is enabled for encode and decode.
- **libjpeg v8 ABI** (`WITH_JPEG8`), so existing libjpeg-based code generally compiles unchanged.
- Built as a **static library**; CLI tools, tests, and the TurboJPEG API are left out to keep flash use down.

## Configuration

- `CONFIG_LIBJPEG_TURBO_ALLOC_PREFER_SPIRAM` (default on when `CONFIG_SPIRAM` is set) — libjpeg-turbo's memory manager allocates from PSRAM first and falls back to internal RAM. This keeps the codec's working buffers out of internal RAM even with `CONFIG_SPIRAM_USE_CAPS_ALLOC`, where `malloc()` never returns PSRAM. Turn it off to keep libjpeg-turbo on plain `malloc()` for speed.

## Add it to your project

From the project directory:

```bash
idf.py add-dependency "espressif/libjpeg-turbo^3.2.0"
```

Or list it in your component manifest (`main/idf_component.yml`):

```yaml
dependencies:
  espressif/libjpeg-turbo: "^3.2.0"
```

Then include the upstream headers and call the libjpeg API:

```c
#include "jpeglib.h"
#include "jerror.h"

struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;

cinfo.err = jpeg_std_error(&jerr);
jpeg_create_decompress(&cinfo);
jpeg_mem_src(&cinfo, jpeg_buf, jpeg_len);
jpeg_read_header(&cinfo, TRUE);
jpeg_start_decompress(&cinfo);
```

The [examples/hello_jpeg](examples/hello_jpeg/README.md) project decodes a bundled JPEG and prints the result — a complete starting point for ESP-IDF.

## API documentation

This component does not invent a new API. Use the upstream manuals:

- [libjpeg-turbo documentation](https://libjpeg-turbo.org/Documentation/Documentation) — project overview and feature notes
- [libjpeg-turbo repository](https://github.com/libjpeg-turbo/libjpeg-turbo)

## License

libjpeg-turbo is under the [IJG](https://spdx.org/licenses/IJG.html) and [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause.html) licenses. Applications that statically link it must include this attribution:

> This software is based in part on the work of the Independent JPEG Group.
