# XMODEM Example

This example demonstrates XMODEM communication over a serial connection between an ESP development board and a host PC. The firmware first receives a transfer into a buffer, then sends the same bytes back. Various XMODEM tools can be used on the host side, such as the provided `tools/xmodem_tool.py`.

## How to Use Example

### Hardware Required

* One board and a USB-to-UART adapter
* Cross-connect the XMODEM UART pins, 8N1, common GND:

```
Adapter TX  --->  Board RX
Adapter RX  <---  Board TX
GND         ----  GND
```

Do not use the console UART (usually UART0) for the transfer unless the console has been moved to USB Serial/JTAG or another port.

### Configure the Example

```
idf.py menuconfig
```

Under **Example Configuration**:

| Option | Meaning |
| --- | --- |
| UART port number | Default: 1 |
| UART TX GPIO | Default: 4 |
| UART RX GPIO | Default: 5 |
| UART baud rate | Default: 115200 |

The receive/echo buffer size is a compile-time macro in `main/xmodem_example_main.c`:

```c
#define EXAMPLE_XYMODEM_BUF_SIZE  2048
```

XMODEM cannot tell payload from trailing `0x1A` padding, so the buffer stores padding as well. The transfer is aborted if a packet would overflow the buffer.

After reset the firmware repeatedly: receive one XMODEM transfer, then send it back.

### Host tool (PC)

`tools/xmodem_tool.py` sends a 0, 1, 2, ... pattern over the USB-UART adapter, then receives the echo and checks it.

```
pip install -r tools/requirements.txt
python tools/xmodem_tool.py -p /dev/ttyUSB1 2048
```

Replace `/dev/ttyUSB1` with the USB-UART port. The length argument is optional (default 2048).

### Build and Flash

```
cd xymodem/examples/xmodem
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

## Example Output

```
I (103) xmodem_example: Receiving into 2048-byte buffer
I (193) xmodem_example: Received packet 1, 1024 bytes
I (283) xmodem_example: Received packet 2, 1024 bytes
I (293) xmodem_example: Receive finished, 2048 bytes
I (293) xmodem_example: Sending 2048 bytes back
I (293) xmodem_example: Sending packet 1, 1024 bytes
I (383) xmodem_example: Sending packet 2, 1024 bytes
I (483) xmodem_example: Send finished
```
