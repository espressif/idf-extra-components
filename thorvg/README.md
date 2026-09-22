# ThorVG component

[![Component Registry](https://components.espressif.com/components/espressif/thorvg/badge.svg)](https://components.espressif.com/components/espressif/thorvg)

This component integrates [ThorVG](https://github.com/thorvg/thorvg) with ESP-IDF and exposes its C API through `thorvg_capi.h`. It uses ThorVG's CPU renderer and supports optional loaders configured at build time.

## Install

Install `espressif/thorvg` from the [ESP Component Registry](https://components.espressif.com/components/espressif/thorvg) using the ESP-IDF Component Manager.

## Build requirements

- ESP-IDF >= 5.1
- [Meson](https://mesonbuild.com) >= 1.3. The build checks the version and stops with an error if an older Meson is found: Meson < 1.3 silently ignores changed `-D` options when `meson setup` re-runs, so flag changes from `sdkconfig` would never reach ThorVG. If Meson is missing entirely, it is installed automatically into the active ESP-IDF Python environment with pip.

## Configure

Run `idf.py menuconfig` and open **Component config → ThorVG Support Options** to configure:

- Loader support (Lottie, SVG, PNG, JPEG, WebP, and fonts)
- Log output

### Multithreading

Multithreading is always on. The build hard-wires Meson's `-Dthreads=true` and lists `pthread` in the component's requirements unconditionally (ESP-IDF is a multi-threaded RTOS environment).

## API and examples

Include `thorvg_capi.h` in your application. For API usage and reference, see the [official ThorVG native API documentation](https://www.thorvg.org/native-apis).

A complete ESP-IDF example is available in [examples/thorvg_lottie](examples/thorvg_lottie/README.md).
