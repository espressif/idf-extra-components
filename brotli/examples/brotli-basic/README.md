# Brotli Basic Example

This example demonstrates the basic Brotli C API on an ESP target:

1. Compress an in-memory text buffer with `BrotliEncoderCompress()`.
2. Decompress the resulting buffer with `BrotliDecoderDecompress()`.
3. Compare the decoded bytes with the original input.

Brotli is a lossless compression algorithm, not an encryption algorithm.
The example uses a 64 KiB compression window (`lgwin=16`) so it can run on targets without PSRAM. Larger windows improve compression for larger inputs, but require substantially more working memory.
The example uses quality 1 so the encoder working memory fits targets without PSRAM, including ESP32-C3. Applications that use higher quality levels need correspondingly more heap.

## Hardware Required

Any ESP32-series chip. No external hardware is required.

## Build and run

```bash
idf.py set-target esp32
idf.py build flash monitor
```

Expected output includes:

```text
I (...) brotli-basic: Compressed ... bytes to ... bytes
I (...) brotli-basic: Brotli round-trip verified
```
