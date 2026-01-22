# Common I/O for esp_linenoise Examples

This folder provides functions to configure and select the input/output file descriptors for esp_linenoise examples.
- On ESP-IDF, it configures UART for use with linenoise.
- On Linux, it uses default stdin/stdout.

## Usage
Include common_io.h in your example and use:
- esp_linenoise_get_default_in_fd()
- esp_linenoise_get_default_out_fd()
- esp_linenoise_configure_uart() (ESP-IDF only)

This ensures all examples work on both ESP-IDF and Linux environments.
