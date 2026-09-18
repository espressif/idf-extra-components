# XYMODEM Test

This test application covers the `xymodem` component. XMODEM cases run `xmodem_send()` and `xmodem_recv()` on two UART ports of the same chip. The ports are cross-connected through the GPIO matrix, so no external jumper wires are required:

```
sender TX  --(GPIO_A)-->  receiver RX
sender RX  <--(GPIO_B)--  receiver TX
```

The console UART (usually UART0) is left untouched. The default ports are UART1 and UART2, which means the target needs at least three UART controllers.

YMODEM cases are not included yet. When added, they should go in `main/test_ymodem.c`.

## Pins and ports

Defaults can be changed in `idf.py menuconfig` under **XYMODEM Test**:

| Option | Default |
| --- | --- |
| Sender UART | 1 |
| Receiver UART | 2 |
| GPIO A (sender TX / receiver RX) | 4 |
| GPIO B (receiver TX / sender RX) | 5 |
| Baud rate | 115200 |

Choose GPIOs that are not used by flash, PSRAM, or strapping-at-boot if the board cannot tolerate those pins toggling after reset.

## Build and run

From the ESP-IDF environment:

```
cd xymodem/test_apps
idf.py set-target esp32s3
idf.py build flash monitor
```

In the Unity menu, press `*` to run all cases, or select an individual case.

pytest:

```
pytest --target esp32s3
pytest --target esp32p4
```
