# FreeType Library

[![Component Registry](https://components.espressif.com/components/espressif/freetype/badge.svg)](https://components.espressif.com/components/espressif/freetype)

This is an IDF component that ports the [FreeType](https://freetype.org/) library into ESP-IDF.

**FreeType** is a software font engine designed to support a large variety of font formats, including TrueType, OpenType, Type1, and more. It is used widely as a rendering library for text and graphics on embedded devices.

## Features

- Load and render fonts from filesystems (SPIFFS, LittleFS, FAT, etc.) or from memory.
- Support for a wide range of font formats (TrueType, OpenType, Type1, CFF, etc.).
- Vector-to-raster conversion with hinting for quality text rendering.

## Usage

Add `espressif/freetype` to the `dependencies` of your project:

```yaml
dependencies:
  espressif/freetype: "^2.14.3"
```

Include the FreeType header and use the API as usual:

```c
#include "freetype/freetype.h"
#include "freetype/ftglyph.h"
```

This component disables optional dependencies (HarfBuzz, BZIP2, Brotli, PNG, and ZLIB) by default to keep the binary size small. Note that the **WOFF2** font format relies on Brotli decompression and is therefore **not supported** in this configuration. Please use TTF/OTF instead.

## Example

An example is available in [examples/freetype-example](examples/freetype-example/README.md), which loads the DejaVu Sans font from filesystem and renders the text "FreeType" into the console as ASCII art.

## Documentation

For usage instructions, please refer to the official documentation: https://freetype.org/freetype2/docs/documentation.html
