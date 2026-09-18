# XMODEM Example

This example demonstrates XMODEM communication over a serial connection between an ESP development board and a host PC. Various XMODEM tools can be used on the host side, such as the provided `tools/xmodem_tool.py`, which is implemented using the Python `xmodem` library. Alternatively, two boards can be used for board-to-board communication.

Since the protocol layer implemented by this component is decoupled from the underlying transport, the example demonstrates two transport implementations: one based on the UART driver and the other on the UHCI (UART DMA) driver.

## How to Use Example

### Hardware Required

* One board and a USB-to-UART adapter, or two boards
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
| XMODEM role | **Send** or **Receive** |
| UART port / TX / RX / baud | Physical link. Defaults: UART1, GPIO4, GPIO5, 115200 |
| Transport backend | **UART driver** or **UHCI (UART DMA)**. UHCI needs `SOC_UHCI_SUPPORTED` and UHCI continuous API |

Please note that the UHCI continuous API (`uhci_start_receive_continuous()` and `uhci_stop_receive()`) may not currently be available in all IDF versions.

The send payload length is a compile-time macro in `main/xmodem_example_main.c`:

```c
#define EXAMPLE_XYMODEM_PAYLOAD_LEN  2048
```

The sender fills the buffer with `0, 1, 2, ..., 255, 0, ...`. The receiver prints every delivered byte as hex. XMODEM cannot tell payload from trailing `0x1A` padding, so those padding bytes are printed as well.

Configure one side as the sender and the other as the receiver. After reset the firmware waits for `s` on the console, then starts.

### Host tool (PC)

`tools/xmodem_tool.py` is a send/receive helper built on the Python `xmodem` package. It talks XMODEM over the USB-UART adapter connected to the board.

```
pip install -r tools/requirements.txt
```

ESP receives, PC sends the same 0, 1, 2, ... pattern as the firmware:

```
python tools/xmodem_tool.py -p /dev/ttyUSB1 send --pattern 2048
```

ESP sends, PC receives and checks the pattern:

```
python tools/xmodem_tool.py -p /dev/ttyUSB1 recv --verify-pattern
```

Replace `/dev/ttyUSB1` with the actual port of the USB-UART adapter. `--mode xmodem1k` is the default and matches the example's mixed/1K blocks. Use `--mode xmodem` for 128-byte blocks only.

Start the board (press `s`) and the host tool so that the receiver begins the handshake.

### Build and Flash

```
cd xymodem/examples/xmodem
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

## Example Output

Sender:

```
Press 's' to start...
I (xxx) xmodem_example: Sending 2048 bytes (0, 1, 2, ...)
I (xxx) xmodem_example: Send finished
```

Receiver:

```
Press 's' to start...
I (xxx) xmodem_example: Receiving. Starting handshake.
00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
...
I (xxx) xmodem_example: Receive finished, 2048 bytes delivered to the sink (includes padding)
```
