# Swift Support Component for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/swift/badge.svg)](https://components.espressif.com/components/espressif/swift)

This component provides [Embedded Swift](https://www.swift.org/documentation/embedded-swift/) language support for ESP-IDF projects targeting RISC-V based ESP chips.

## Features

- Configures the Swift compiler for cross-compilation to RISC-V ESP targets
- Provides `idf_component_register_swift()` CMake function for easy integration of Swift source files into ESP-IDF components
- Handles bridging header configuration for C/Swift interoperability
- Automatically extracts and applies RISC-V architecture flags from the ESP-IDF toolchain

## Supported Targets

Embedded Swift is supported on **RISC-V** based ESP chips only (e.g. ESP32-C3, ESP32-C6, ESP32-H2). Xtensa are **not** supported.

## Usage

Add this component as a dependency in your project, then use the `idf_component_register_swift()` function in your component's `CMakeLists.txt`:

```cmake
# Need to register empty IDF component first
idf_component_register(
    SRCS /dev/null
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES swift
)

idf_component_register_swift(${COMPONENT_LIB}
    BRIDGING_HEADER "BridgingHeader.h"
    SRCS "Main.swift"
)
```

### Parameters

- **BRIDGING_HEADER** — Path to the C bridging header file that exposes C APIs to Swift code. Required.
- **SRCS** — List of Swift source files to compile. Required.

## Examples

- https://github.com/swiftlang/swift-embedded-examples/tree/main
