| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash Example

This example demonstrates how to use the SPI NAND Flash driver with FAT filesystem in ESP-IDF.

## Hardware Required

* Any ESP board from the supported targets list above
* An external SPI NAND Flash chip connected to the following pins:
  * For ESP32 (SPI3):
    - MOSI - SPI3_IOMUX_PIN_NUM_MOSI (23)
    - MISO - SPI3_IOMUX_PIN_NUM_MISO (19)
    - CLK  - SPI3_IOMUX_PIN_NUM_CLK (18)
    - CS   - SPI3_IOMUX_PIN_NUM_CS (5)
    - WP   - SPI3_IOMUX_PIN_NUM_WP (22)
    - HD   - SPI3_IOMUX_PIN_NUM_HD (21)
  * For other ESP chips (SPI2):
    - MOSI - SPI2_IOMUX_PIN_NUM_MOSI (13)
    - MISO - SPI2_IOMUX_PIN_NUM_MISO (12)
    - CLK  - SPI2_IOMUX_PIN_NUM_CLK (14)
    - CS   - SPI2_IOMUX_PIN_NUM_CS (15)
    - WP   - SPI2_IOMUX_PIN_NUM_WP (2)
    - HD   - SPI2_IOMUX_PIN_NUM_HD (4)

## Configuration

The example can be configured to format the filesystem if mounting fails. This can be enabled using the `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` configuration option.

## How to Use Example

Build the project and flash it to the board, then run monitor tool to view serial output:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

The example:
1. Initializes the SPI bus and NAND Flash device
2. Mounts a FAT filesystem on the NAND Flash
3. Creates and writes to a file named "hello.txt"
4. Reads back and displays the contents of the file
5. Displays filesystem space information
6. Unmounts the filesystem and deinitializes the NAND Flash

Here is the example's console output:
```
...
I (315) main_task: Calling app_main()
I (315) example: DMA CHANNEL: 3
W (355) vfs_fat_nand: f_mount failed (13)
I (355) vfs_fat_nand: Formatting FATFS partition, allocation unit size=16384
I (6635) vfs_fat_nand: Mounting again
I (6655) example: FAT FS: 117024 kB total, 117024 kB free
I (6655) example: Opening file
I (6685) example: File written
I (6685) example: Reading file
I (6685) example: Read from file: 'Written using ESP-IDF v5.5-dev-2627-g2cbfce97768'
I (6685) example: FAT FS: 117024 kB total, 117008 kB free
I (6695) gpio: GPIO[5]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6705) gpio: GPIO[23]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6705) gpio: GPIO[19]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6715) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6725) gpio: GPIO[22]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6735) gpio: GPIO[21]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (6745) main_task: Returned from app_main()
...
```
