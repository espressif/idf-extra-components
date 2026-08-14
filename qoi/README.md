# QOI - The "Quite OK Image" format for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/qoi/badge.svg)](https://components.espressif.com/components/espressif/qoi)

This component ports the [QOI](https://github.com/phoboslab/qoi) single-header library into ESP-IDF.

**QOI** (Quite OK Image) is a fast, lossless image compression format. Compared to PNG it offers a similar compression ratio but significantly higher encoding/decoding throughput, which makes it a great fit for memory-to-memory image processing on embedded targets such as:

- Screen-capture / screenshot-like operations in LCD applications
- Encoding framebuffers for fast storage or transfer
- On-the-fly lossless image compression where speed matters more than the
  (slightly worse than libpng) compression ratio

## Supported API

- `qoi_encode()` — encode raw RGB/RGBA pixels into a QOI image **in memory**
- `qoi_decode()` — decode a QOI image from memory
- `qoi_write()`  — encode and write a QOI image to a file
- `qoi_read()`   — read and decode a QOI image from a file
