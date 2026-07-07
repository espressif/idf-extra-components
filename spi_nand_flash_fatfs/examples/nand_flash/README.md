| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash Example

This example demonstrates how to use the SPI NAND Flash driver with FAT filesystem in ESP-IDF.

Canonical source: [`main/spi_nand_flash_example_main.c`](main/spi_nand_flash_example_main.c).

## Use in your own project

### 1. Add the component dependency

```bash
idf.py add-dependency "espressif/spi_nand_flash_fatfs"
```

Or add to your project's `idf_component.yml`:

```yaml
dependencies:
  espressif/spi_nand_flash_fatfs:
    version: "*"
```

This also resolves `espressif/spi_nand_flash` transitively.

### 2. Declare the component in CMake

In your main component's `CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES spi_nand_flash_fatfs)
```

### 3. Required headers

Match the includes in [`main/spi_nand_flash_example_main.c`](main/spi_nand_flash_example_main.c):

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "esp_vfs_fat_nand.h"
```

`esp_vfs_fat_nand.h` pulls in `spi_nand_flash.h` and the SPI driver headers transitively.

### 4. Initialization flow

Follow the same order as `example_init_nand_flash()` in [`main/spi_nand_flash_example_main.c`](main/spi_nand_flash_example_main.c):

1. **SPI bus** — `spi_bus_initialize()` with `spi_bus_config_t` (MOSI, MISO, CLK, WP, HD GPIOs).
2. **SPI device** — `spi_bus_add_device()` with `spi_device_interface_config_t` (clock, CS, mode, `flags`).
3. **NAND driver** — fill `spi_nand_flash_config_t` and call `spi_nand_flash_init_device()`.
4. **FAT mount** — `esp_vfs_fat_nand_mount()` with `esp_vfs_fat_mount_config_t`.
5. **File I/O** — standard `fopen()` / `fprintf()` / `fgets()` on paths under the mount point (e.g. `/nandflash/hello.txt`).
6. **Cleanup** — `esp_vfs_fat_nand_unmount()`, then `spi_nand_flash_deinit_device()`, `spi_bus_remove_device()`, `spi_bus_free()` (see `example_deinit_nand_flash()`).

Default pin macros (`HOST_ID`, `PIN_MOSI`, etc.) are defined at the top of the example source. Adjust them for your board or use the tables in [Hardware Required](#hardware-required) below.

### 5. `spi_nand_flash_config_t`

Populated in `example_init_nand_flash()` before `spi_nand_flash_init_device()`:

| Field | Description |
|-------|-------------|
| `device_handle` | `spi_device_handle_t` returned by `spi_bus_add_device()` |
| `io_mode` | `SPI_NAND_IO_MODE_SIO` in this example; also `DIO`, `DOUT`, `QIO`, or `QOUT` |
| `flags` | `SPI_DEVICE_HALFDUPLEX` for half-duplex (required for DIO/DOUT); `0` for full-duplex SIO. **Must match** `spi_device_interface_config_t.flags` |
| `gc_factor` | Optional wear-leveling GC tuning; omit or set `0` for the driver default |

Full struct documentation is in [`spi_nand_flash.h`](../../../spi_nand_flash/include/spi_nand_flash.h).

### 6. Mount configuration

```c
esp_vfs_fat_mount_config_t config = {
    .max_files = 4,
    .format_if_mount_failed = false,  // or true to format on first mount failure
    .allocation_unit_size = 16 * 1024,
};
esp_err_t ret = esp_vfs_fat_nand_mount("/nandflash", flash, &config);
```

In this example project, `format_if_mount_failed` is controlled by `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` in menuconfig.

### 7. Prerequisites

- Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled** (Component config → SPI NAND Flash). `spi_nand_flash_init_device()` returns `ESP_ERR_NOT_SUPPORTED` when BDL is enabled, and FatFs mount helpers require the legacy handle.
- See also [`spi_nand_flash_fatfs` component README](../../README.md#requirements-read-first).

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

Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled** (Component config → SPI NAND Flash). This example uses `spi_nand_flash_init_device()`, which is not available when BDL is enabled.

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
