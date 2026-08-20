# ThorVG component

[![Component Registry](https://components.espressif.com/components/espressif/thorvg/badge.svg)](https://components.espressif.com/components/espressif/thorvg)

This component integrates [ThorVG](https://github.com/thorvg/thorvg) with ESP-IDF and exposes its C API through `thorvg_capi.h`. It uses ThorVG's CPU renderer and supports optional loaders configured at build time.

## Install

Install `espressif/thorvg` from the [ESP Component Registry](https://components.espressif.com/components/espressif/thorvg) using the ESP-IDF Component Manager.

## Configure

Run `idf.py menuconfig` and open **Component config → ThorVG Support Options** to configure:

- Loader support (Lottie, SVG, PNG, JPEG, WebP, and fonts)
- Multithreading
- Log output

## API and examples

Include `thorvg_capi.h` in your application. For API usage and reference, see the [official ThorVG native API documentation](https://www.thorvg.org/native-apis).

A complete ESP-IDF example is available in [examples/thorvg_lottie](examples/thorvg_lottie/README.md).
