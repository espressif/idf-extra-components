# UART Emulator Example (RMT backend)

This example demonstrates how to emulate UART using the [uart_emu](https://components.espressif.com/component/espressif/uart_emu) component.

## How to Use Example

### Hardware Required

* A development board with Espressif SoC
* A USB cable for Power supply and programming
* A UART bridge (optional, set the TX and RX pins to be the same as the loopback test if don't have)

### Configure the Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`. Then assign the proper GPIO in the [source file](main/uart_emu_main.c).

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output (loopback test)

```text
I (248) main_task: Started on CPU0
I (248) main_task: Calling app_main()
I (258) uart_emu_rmt: new uart emu at 0x4087cd38, baud=115200  rmt_resolution=921600
I (268) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (278) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (288) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (298) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (308) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (318) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (328) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (338) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (348) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (358) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (368) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (378) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (388) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (398) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (408) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (418) uart_emu_rmt_example: Read len: 23, data: RMT UART, transmission
I (478) uart_emu_rmt_example: UART transmit 16 times!
I (478) main_task: Returned from app_main()

```
