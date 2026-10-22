# BDC Motor Get Started

This example demonstrates how to drive a brushed DC motor with the [bdc_motor](https://components.espressif.com/component/espressif/bdc_motor) component and its MCPWM backend.

The application creates a motor handle, enables it, sets a 50% duty cycle, then cycles through **forward**, **reverse**, **coast**, and **brake**.

## How to Use Example

### Hardware Required

* A development board with an Espressif SoC that has MCPWM (for example ESP32, ESP32-S3, or ESP32-C5)
* A USB cable for power supply and programming
* An H-bridge motor driver (for example DRV8871 or L298N)
* A brushed DC motor

Connect the example PWM pins to the H-bridge inputs:

| Signal | Default GPIO |
|--------|--------------|
| PWMA / IN1 | GPIO 4 |
| PWMB / IN2 | GPIO 5 |

Change `BDC_MCPWM_GPIO_A` and `BDC_MCPWM_GPIO_B` in [bdc_motor_example_main.c](main/bdc_motor_example_main.c) if your wiring is different.

### Configure the Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```text
I (309) example: Create DC motor
I (319) example: Enable motor
I (319) example: Set speed to 50%
I (319) example: Motor forward
I (819) example: Motor reverse
I (1319) example: Motor coast
I (1819) example: Motor brake
```
