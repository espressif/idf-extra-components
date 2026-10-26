# Quirc QR code decoding library

[![Component Registry](https://components.espressif.com/components/espressif/quirc/badge.svg)](https://components.espressif.com/components/espressif/quirc)

This component is the ESP-IDF packaging of [quirc](https://github.com/dlbeer/quirc), a small C library that finds and decodes QR codes in images. It is QR-only: 1D barcodes (EAN, Code 128, and similar) and other 2D formats (Data Matrix, Aztec, PDF417) are out of scope.

Use it to turn a grayscale camera frame or a still image into the payload of one or more QR codes. To *generate* a QR code instead, use the [`qrcode`](https://components.espressif.com/components/espressif/qrcode) component.

## Features

- **Identify and decode** QR codes from an 8-bit grayscale image through `quirc.h`.
- **Robust recognition**: codes that are rotated or viewed at an angle still decode.
- **Several codes in one image** — `quirc_count()` / `quirc_extract()` walk every detection.
- **No extra dependencies** beyond standard C.
- **Small working set**: one byte per image pixel, plus a few kilobytes per decoder object.

## Add it to your project

From the project directory:

```bash
idf.py add-dependency "espressif/quirc^1.2.0"
```

Or list it in your component manifest (`main/idf_component.yml`):

```yaml
dependencies:
  espressif/quirc: "^1.2.0"
```

Then include the upstream header:

```c
#include "quirc.h"
```

## On ESP-IDF

- The API takes 8-bit grayscale only (one byte per pixel), not RGB. Convert a color camera frame or a colored QR image to luminance first; quirc then thresholds dark vs light modules. A 640×480 buffer is about 300 KiB — prefer PSRAM at that size.
- Identification and decode use a large task stack (about 10 KiB for a small code, more for larger versions). Run them on a dedicated FreeRTOS task rather than the default `app_main` stack.
- This port already builds with `QUIRC_FLOAT_TYPE=float` and `QUIRC_USE_TGMATH`, so the decoder uses single-precision math on chips that have only an SP FPU.

## API documentation

This component does not invent a new API. For function contracts, the identify/decode pipeline, mirrored-code handling (`quirc_flip`), and compile-time options, use the upstream programming notes:

- [Library use](https://github.com/dlbeer/quirc#library-use) — create, resize, identify, extract, and decode
- [`quirc.h`](https://github.com/dlbeer/quirc/blob/master/lib/quirc.h) — the full C API, documented in the header

## License

quirc is distributed under the ISC License. See [LICENSE](LICENSE).
