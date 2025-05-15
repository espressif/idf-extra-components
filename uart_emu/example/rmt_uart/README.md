# UART Emulator Example (RMT backend)

This example demonstrates how to simulate UART using the [uart_emu](https://components.espressif.com/component/espressif/uart_emu) component.

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
I (258) rmt-uart: new rmt uart at 0x4087cd38, baud=115200  rmt_resolution=921600
I (268) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (278) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (288) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (298) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (308) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (318) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (328) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (338) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (348) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (358) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (368) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (378) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (388) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (398) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (408) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (418) rmt_uart_example: Read len: 23, data: RMT UART, transmission
I (478) rmt_uart_example: UART transmit 16 times!
I (478) main_task: Returned from app_main()

```
