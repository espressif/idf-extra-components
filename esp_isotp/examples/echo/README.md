# ISO-TP Echo Example

Simple ISO-TP echo service: receives data and sends it back. Supports single-frame and multi-frame transfers.

## How to Use Example

### Hardware Required

* An ESP32 development board
* A transceiver (e.g., TJA1050)
* An USB cable for power supply and programming

### Configuration

Use `idf.py menuconfig` to configure the example:

- **ISO-TP Echo Configuration → TWAI Basic Configuration**:
  - TX GPIO Number (default: GPIO 5)
  - RX GPIO Number (default: GPIO 4)
  - TWAI Bitrate (default: 500000)

- **ISO-TP Echo Configuration → ISO-TP Configuration**:
  - TX/RX Message IDs (default: 0x7E0/0x7E8)
  - Buffer sizes

Connect the ESP32 to a CAN transceiver and the CAN bus.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Example Output

Once the application is running, you will see the following output:

```
I (xxx) isotp_echo: ISO-TP Echo Demo started
I (xxx) isotp_echo: ISO-TP echo example's TX ID: 0x7E0, RX ID: 0x7E8
```

To test the echo functionality, you can use a tool like `can-utils` on a Linux machine connected to the same CAN bus:

```bash
# Send a message and wait for the echo (using default IDs from Kconfig)
candump -tA -e -c -a vcan0 &
(isotprecv -s 0x7E0 -d 0x7E8 vcan0 | hexdump -C) & (echo 11 22 33 44 55 66 DE AD BE EF | isotpsend -s 0x7E0 -d 0x7E8 vcan0)
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/idf-extra-components/issues) on GitHub. We will get back to you soon.
