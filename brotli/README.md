# Brotli compression and decompression library

[![Component Registry](https://components.espressif.com/components/espressif/brotli/badge.svg)](https://components.espressif.com/components/espressif/brotli)

This is an IDF component that ports the [Brotli](https://github.com/google/brotli) library into ESP-IDF.

**Brotli** is a generic-purpose lossless compression algorithm that compresses data using a combination of a modern variant of the LZ77 algorithm, Huffman coding and 2nd order context modeling. It is particularly well suited for compressing font data, web content and other text-like payloads, and is required to decompress the **WOFF2** font format.

This component builds only the Brotli **C library** (as a static library) from the upstream [google/brotli](https://github.com/google/brotli) repository. Java, Python, JavaScript and other bindings are not included.

## Features

- Full Brotli C API: encoding (`brotli/encode.h`) and decoding (`brotli/decode.h`).
- Compiled as a static library for embedded use; CLI tools, tests and documentation are disabled.

## Usage

Add `espressif/brotli` to the `dependencies` of your project:

```yaml
dependencies:
  espressif/brotli: "^1.2.0"
```

Include the Brotli headers and use the API as usual:

```c
#include "brotli/decode.h"
#include "brotli/encode.h"
```

## Example

The [examples/brotli-basic](examples/brotli-basic) project demonstrates compressing an in-memory buffer, decompressing it, and verifying the round-trip result.

## Documentation

For usage instructions, please refer to the official documentation: https://brotli.org/
