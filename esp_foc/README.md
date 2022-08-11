# espFoC: Vector FoC controller for PMSM motors for ESP-IDF

![Build](https://github.com/uLipe/espFoC/workflows/Build/badge.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

espFoC is a simple implementation of voltage mode, vector controller intended to be used with permanent-magnet synchronous motors (PMSM), and general brushless motors. This component was developed to be used with the ESP-IDF 
espressif framework.

## Getting started:
* Just clone this project on most convenient folder;
* Inside of your IDF project CMakeLists.txt set or add the path of this component to EXTRA_COMPONENT_DIRS for example: `set(EXTRA_COMPONENT_DIRS "path/to/this/component/")`
* For batteries included getting started, refer the examples folder.

## Features:
* Voltage mode control, control a PMSM like a DC motor!;
* Position and Speed closed-loop control;
* Single-precision Floating point implementation;
* Sample inverter driver based on esp32 LEDC PWM (easy to wire!);
* Sample rotor position driver based on as5600 encoder (very popular!);
* FoC engine runs sychronized at inverter PWM rate;

## Limitations:
* Support for esp32 and esp32s3 only;
* Requires and rotor position sensor, for example, incremental encoder.

## Support:
If you find some trouble, open an issue, and if you are enjoying the project
give it a star. Also, you can try reaching me at ryukokki.felipe@gmail.com