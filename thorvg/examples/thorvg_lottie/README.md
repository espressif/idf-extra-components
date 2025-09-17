# ThorVG Lottie Animation Example

This is a minimalistic display + ThorVG graphics library example that demonstrates how to render Lottie animations on an ESP device. With just a few function calls, it sets up the display and shows Lottie animations using the ThorVG vector graphics engine.

## Overview

This example demonstrates:
- Setting up an SPI LCD display (SH8601 controller)
- Initializing the ThorVG vector graphics engine
- Loading and rendering a Lottie animation from a JSON file
- Converting graphics buffers from ARGB8888 to RGB565 format for display

## Hardware Required

- An ESP32 device with PSRAM support (necessary for the graphic buffers)
- An SPI LCD display (the example uses SH8601 controller by default)

## Building and Running

1. Prepare your project's partition table to include a LittleFS partition named "storage"
2. Place your Lottie animation file (in JSON format) in the project's "storage" directory with the name "emoji-animation.json"
3. Build and flash the application as usual for an ESP-IDF project:

```
idf.py set-target esp32s3
idf.py -p PORT flash monitor
```

## Customizing the Example

### Using a Different LCD Display

If you are using a different LCD controller, you'll need to:

1. Replace the SH8601-specific initialization code in the `main/thorvg_example_main.c` file:
   - Update the LCD pin configurations
   - Replace the `esp_lcd_new_panel_sh8601()` call with the appropriate function for your LCD
   - Modify the LCD initialization commands as required by your display

### Using a Different Lottie File

To use your own Lottie animation:

1. Ensure your animation file is in JSON format
2. Update the `EXAMPLE_LOTTIE_FILENAME` macro in the code to match your file path
3. Adjust the `EXAMPLE_LOTTIE_SIZE_HOR` and `EXAMPLE_LOTTIE_SIZE_VER` macros if your animation has different dimensions

## Memory Usage

This example allocates graphic buffers in PSRAM:
- One ARGB8888 buffer (4 bytes per pixel) for ThorVG rendering
- One RGB565 buffer (2 bytes per pixel) for LCD output

Ensure your device has sufficient PSRAM for these buffers, especially when increasing animation dimensions.
