# Zlib Compression Library

[![Component Registry](https://components.espressif.com/components/espressif/zlib/badge.svg)](https://components.espressif.com/components/espressif/zlib)

This ESP-IDF component packages [zlib](https://zlib.net/), the general-purpose lossless data-compression library. It provides the classic `deflate`/`inflate` APIs as well as the gzip-compatible streaming APIs (`gzopen`, `gzread`, ...), so you can compress and decompress data on ESP chips and interoperate with `.gz` files and other `deflate`-based formats (PNG, HTTP `Content-Encoding: deflate`, ZIP, and more).

## Memory Configuration

- `CONFIG_ZLIB_PREFER_PSRAM` (default on when `CONFIG_SPIRAM` is set) - the memory zlib allocates for a `z_stream` (deflate/inflate state, windows, hash chains, pending buffer) comes from `heap_caps`: requests of 16 KiB or more prefer PSRAM, smaller ones prefer internal RAM, and either side falls back to the other. Turn it off to keep those allocations on plain `malloc()` for speed.

The gzip file APIs (`gzopen`, `gzread`, `gzwrite`) still call `malloc()` for their own file state and I/O buffers; this option does not redirect those.

Only the `z_stream` state and buffers are affected. The buffers an application passes to `compress2()`, `deflate()` or `inflate()` belong to the application and are allocated by the application. An allocator installed in `z_stream.zalloc`/`zfree` before `deflateInit()` or `inflateInit()` always takes precedence over the default, as zlib documents, and an application that supplies no allocator leaves the component's routing in place.

## Using the Component

Add this component to your project with the [IDF Component Manager](https://docs.espressif.com/projects/idf-component-manager/en/latest/):

```yaml
dependencies:
  espressif/zlib: "^1.3.2"
```

or run `idf.py add-dependency "espressif/zlib^1.3.2"` in your project. Then include the upstream header:

```c
#include "zlib.h"
```

## API

This component exposes zlib's own API. Function contracts, error codes, the one-shot helpers and the streaming interface are in the [official zlib manual](https://www.zlib.net/manual.html).
