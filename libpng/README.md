# libpng: the PNG codec, packaged for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/libpng/badge.svg)](https://components.espressif.com/components/espressif/libpng)

This component is the ESP-IDF packaging of [libpng](https://github.com/pnggroup/libpng), the official reference implementation of the Portable Network Graphics (PNG) format. It is not a new API: include the upstream headers and call the classic **libpng C API**.

Use it to decode a PNG into RGBA pixels you can push to a display, or to encode a framebuffer into a PNG. libpng is an ISO/IEC 15948 reference implementation. It is built here as the portable C library, as a static archive.

## Features

- **Decode and encode** all PNG image types (grayscale, palette, RGB, alpha, 1/2/4/8/16-bit) through `png.h`.
- **Sequential and progressive** (progressive-read) decoding.
- **Simplified read/write API** (`png_image`) for the common in-memory cases.
- **In-memory I/O** with `png_set_read_fn()` / `png_set_write_fn()`, so no filesystem is required.
- **Built-in transforms**: gamma correction, 16-to-8-bit scaling, palette expansion, alpha compositing, RGB/BGR and interlacing handling.
- Built as a **static library**.
- **The zlib dependency is pulled in automatically.**

## Add it to your project

From the project directory:

```bash
idf.py add-dependency "espressif/libpng^1.6.58"
```

Or list it in your component manifest (`main/idf_component.yml`):

```yaml
dependencies:
  espressif/libpng: "^1.6.58"
```

Then include the upstream header and call the libpng API:

```c
#include "png.h"

png_image image;
memset(&image, 0, sizeof(image));
image.version = PNG_IMAGE_VERSION;

/* Decode a PNG held in memory. */
png_image_begin_read_from_memory(&image, buf, buf_len);
image.format = PNG_FORMAT_RGBA;

void *buffer = malloc(PNG_IMAGE_SIZE(image));
png_image_finish_read(&image, NULL, buffer, 0, NULL);
```

## Configuration

The libpng settings that matter on ESP-IDF are all tunable at runtime on each image, for example:

- `png_set_compression_buffer_size()` — I/O buffer size
- `png_set_user_limits()` — maximum image width and height
- `png_set_chunk_malloc_max()` / `png_set_chunk_cache_max()` — chunk limits

### PSRAM

On targets with PSRAM, `CONFIG_LIBPNG_PREFER_PSRAM` routes libpng's allocations to PSRAM first and falls back to internal RAM.

## API documentation

This component does not invent a new API. Use the upstream manuals:

- [libpng manual](https://libpng.sourceforge.io/) — API reference, one page per function
- [libpng project page](https://www.libpng.org/pub/png/libpng.html) — overview and downloads
- [libpng repository](https://github.com/pnggroup/libpng)
