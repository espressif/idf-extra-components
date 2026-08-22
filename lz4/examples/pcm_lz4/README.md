# LZ4 PCM example

This is a complete ESP-IDF application that demonstrates two common ways to
use LZ4 on the same deterministic, PCM-like `int16_t` sample buffer:

1. **Block API** (`lz4_block_example.c`): compress a complete buffer with
   `LZ4_compress_default()` and restore it with `LZ4_decompress_safe()`.
2. **Frame streaming API** (`lz4_stream_example.c`): create a self-describing
   frame with `LZ4F_compressFrame()` and restore it with repeated
   `LZ4F_decompress()` calls using deliberately small input and output chunks.

`app_main.c` runs the Block example first and then the Frame streaming example,
so the two usage patterns can be read independently. The generated signal is
deliberately repeatable and locally correlated, so it behaves like a useful raw
audio buffer without requiring an input file or a microphone.

Each example checks its result and prints the original/compressed size. Errors
are handled with `ESP_ERROR_CHECK()` to keep the demonstration code focused on
the LZ4 API; an invalid allocation or API result intentionally stops the
application. Performance measurements are intentionally kept out of this
beginner-oriented example; see the test app for benchmark results.

## Run locally on an ESP board

Prerequisites:

- ESP-IDF installed and exported (`idf.py --version` should work).
- An ESP32 or ESP32-C3 board connected over USB.
- The LZ4 submodule initialized when using a checkout of this repository:

  ```bash
  git submodule update --init --recursive
  ```

Build, flash, and open the serial monitor from the repository root:

```bash
idf.py -C lz4/examples/pcm_lz4 set-target esp32
idf.py -C lz4/examples/pcm_lz4 build
idf.py -C lz4/examples/pcm_lz4 -p PORT flash monitor
```

Replace `esp32` and `PORT` with the target and serial port for the board. The
`set-target` command only needs to be run once per example build directory.
Press `Ctrl-]` to exit the monitor.

The output should contain lines similar to these (sizes and timings depend on
the target):

```text
Block API example
source_size=8192 compressed_size=... verify=true
Frame streaming API example
source_size=8192 frame_size=... verify=true
```

The example is also included in the repository's embedded pytest suite; the
script checks the three verification flags and can be run by the project's
usual `pytest`/`idf-build-apps` workflow.
