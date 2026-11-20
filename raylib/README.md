# Raylib, a simple 2D game programming library, running with software renderer

[![Component Registry](https://components.espressif.com/components/espressif/raylib/badge.svg)](https://components.espressif.com/components/espressif/raylib)

[Raylib](https://www.raylib.com/) is a small library for 2D graphics built around a plain C API (`InitWindow()`, `BeginDrawing()`, `DrawText()`, `EndDrawing()`). This component packages raylib for ESP-IDF and runs it on raylib's **software renderer**, so it works on any ESP chip — no GPU required. The 2D drawing API is the upstream one; audio, 3D models, gestures and window/input devices are not adapted yet.

## Add it to your project

```bash
idf.py add-dependency "espressif/raylib^6.2.0"
```

Then include the upstream header plus the component's own:

```c
#include "raylib.h"
#include "esp_raylib.h"
```

## Documentation

The full programming guide — registration, display setup, the flush callback contract, `menuconfig` options and the generated API reference — lives in the online documentation:

- **Programming Guide & API Reference**: [Raylib ESP-IDF Documentation](https://espressif.github.io/idf-extra-components/latest/raylib/index.html)

For the upstream 2D drawing API itself, see the [raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html).

## Example

- [examples/raylib-fireworks](examples/raylib-fireworks) drives an ST7789 panel over SPI on an ESP32-S3 with a self-playing fireworks show over a night skyline.

## License

Upstream raylib (`raylib/`) is [zlib](LICENSE). ESP-IDF packaging, the platform port (`src/`, `include/`) are Apache-2.0.
