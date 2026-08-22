# Fast, lightweight LZ4 compression for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/lz4/badge.svg)](https://components.espressif.com/components/espressif/lz4)

This component packages the upstream [LZ4](https://github.com/lz4/lz4) lossless compression library for ESP-IDF.
It includes the Block, High Compression (HC), and Frame implementations, as well as streaming support.
The component does not add an ESP-specific wrapper, so data compressed by it can be exchanged with standard LZ4 implementations.

LZ4 is a good fit for buffers such as PCM samples, sensor data, and other raw data with local repetition.
It is normally not useful for data that is already compressed, such as JPEG, H.264, or MP3.

The Block API is compact and fast, but the application must store the compressed and original sizes separately.
The High Compression (HC) API can produce a smaller Block payload at the cost of more compression time.
Use the Frame API when the data needs a self-describing format with block metadata and optional content or block checksums.

## Add the component

From an ESP-IDF project, add the registry dependency:

```bash
idf.py add-dependency "espressif/lz4^1.10.0"
```

Alternatively, add this to `main/idf_component.yml`:

```yaml
dependencies:
  espressif/lz4: "^1.10.0"
```

## Use it in a project

Declare the dependency in the component that includes LZ4, then include the upstream headers in your application code:

```c
#include "lz4.h"       // Block API
#include "lz4hc.h"     // HC API (optional)
#include "lz4frame.h"  // Frame API (optional)
```

This component intentionally follows the upstream API and behavior.
For function contracts, streaming examples, format details, and security notes, read the official [LZ4 documentation](https://github.com/lz4/lz4/tree/dev/doc) and the headers (`lz4.h`, `lz4hc.h`, and `lz4frame.h`).

## Configure memory allocation

HC compression requires a relatively large workspace and needs a sufficiently large contiguous memory block.
On memory-constrained targets, use the component's Kconfig option to select the preferred memory region:

```text
LZ4 Configurations → Prefer PSRAM for LZ4 internal allocations
```

This option is also available as:

```text
CONFIG_LZ4_USE_PSRAM=y
```

When enabled, LZ4 prefers PSRAM for its internal allocations and falls back to internal 8-bit RAM when PSRAM is unavailable or cannot satisfy a request.

## About License

The upstream LZ4 library sources included in this component come from the `lib` directory and are distributed under the BSD 2-Clause License.
This component does not use the other, non-BSD-licensed parts of the upstream LZ4 repository, such as the command-line tools and other repository content.

In addition to the upstream library, this component contains ESP-IDF porting and integration code.
That code is distributed under the Apache License 2.0.

The applicable license texts are included in [`LICENSE`](LICENSE).
