# Raylib Hello World Example

Simple raylib example displaying colored text on the screen.

## Building

Build with ESP-IDF:
```bash
idf.py build
```

Flash to device:
```bash
idf.py flash monitor
```

## Hardware

This example is configured for specific development boards with display support.
See the board-specific documentation for pin configurations and display details.

## Wokwi Simulation

This example also supports Wokwi simulation for testing without hardware.

Install wokwi-cli: https://docs.wokwi.com/wokwi-ci/cli-installation

Build and run:
```bash
idf.py build
wokwi-cli
```
