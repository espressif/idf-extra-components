# QOI Image Encode Example

This example demonstrates how to use the `qoi` component to encode an in-memory RGBA image into a QOI bitstream — mimicking an LCD "screenshot" that captures a framebuffer and compresses it losslessly and very fast.

The example:

1. Generates a synthetic RGBA framebuffer (160x120) directly on the CPU.
2. Encodes it into an sRGB QOI bitstream with `qoi_encode()`.
3. base64-encodes the bitstream with mbedTLS.
4. Streams the metadata and base64 payload over the serial console.

The `pytest_qoi_encode.py` script runs against the example, validates the reported image metadata, reassembles the base64 payload into a `.qoi` image, and saves it to the test log directory as `qoi_encode_result.qoi`.

## Hardware Required

Any ESP32-series chip. No external hardware is required.

## How to build and run

```bash
idf.py set-target esp32
idf.py build flash monitor
```

The serial console will print output similar to:

```
Generated 160x120 RGBA framebuffer: 76800 bytes
Encoded QOI size: 1406 bytes (ratio 1.83%)
QOI_META width=160 height=120 channels=4 colorspace=0 encoding=base64 size=1406
QOI_BASE64_BEGIN
QOI_BASE64 ...
QOI_BASE64_END
QOI encode demo done.
```

`colorspace=0` denotes `QOI_SRGB`, which is the color space used by this example's simulated display framebuffer.

## Running the host-side verification

```bash
pytest pytest_qoi_encode.py --target esp32 --port PORT
```

Replace `esp32` with another supported target such as `esp32s3` or `esp32c3`, and set `PORT` to your board's serial device.

The reconstructed artifact is stored in pytest-embedded's per-test log directory (typically under `/tmp/pytest-embedded/`). The test log includes a line similar to:

```
LOG: Saved QOI artifact to /tmp/pytest-embedded/<test_name>/qoi_encode_result.qoi
```

You can open the generated `qoi_encode_result.qoi` from that log directory with a proper image viewer to inspect the QOI output locally.
