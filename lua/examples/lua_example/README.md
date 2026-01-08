# Lua Example

This example demonstrates how to embed Lua in an ESP-IDF application.

## Features

- **Embedded Lua script execution**: Run Lua code directly from C strings
- **File-based Lua script execution**: Load and execute Lua scripts from LittleFS filesystem
- **Memory tracking**: Monitor memory usage throughout Lua operations
- **Error handling**: Proper error handling and reporting

## Example Contents

1. **Simple Embedded Script**: A basic Lua script that calculates and prints a value (answer = 42)
2. **Fibonacci Script**: A Lua script loaded from filesystem that calculates Fibonacci numbers

## Hardware Requirements

- Any ESP32 series board (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, etc.)
- Minimum 4MB flash (for filesystem partition)

## Configuration

- The example uses LittleFS to store Lua scripts
- A custom partition table is included with an "assets" partition for Lua scripts

## Building and Flashing

```bash
idf.py build flash monitor
```

## Expected Output

```
I (xxx) lua_example: Lua Example Starting
I (xxx) lua_example: Starting Lua test: Simple Embedded Script
The answer is: 42
I (xxx) lua_example: End of Lua test: Simple Embedded Script
I (xxx) lua_example: Starting Lua test from file: Fibonacci Script from File
Fibonacci of 10 is: 55
I (xxx) lua_example: End of Lua test from file: Fibonacci Script from File
I (xxx) lua_example: End of Lua example application.
```

## Customization

You can add your own Lua scripts to the `assets/` directory and execute them from your application code.

## See Also

- [Lua Component Documentation](../../README.md)
- [Lua Official Website](https://www.lua.org/)
- [Lua Reference Manual](https://www.lua.org/manual/5.5/)
