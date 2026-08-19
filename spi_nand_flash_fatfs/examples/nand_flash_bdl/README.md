| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash — FatFS via BDL (Block Device Layer)

This example is like [examples/nand_flash](../nand_flash/README.md), but it mounts FatFS using ESP-IDF’s generic BDL path:

- **`spi_nand_flash_init_with_layers()`** builds **Flash BDL → WL BDL** (Dhara wear leveling).
- **`esp_vfs_fat_bdl_mount()`** / **`esp_vfs_fat_bdl_unmount()`** connect VFS + FatFS to that WL block device.

Canonical source: [`main/spi_nand_flash_bdl_example_main.c`](main/spi_nand_flash_bdl_example_main.c).

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

Match the includes in [`main/spi_nand_flash_bdl_example_main.c`](main/spi_nand_flash_bdl_example_main.c):

```c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_blockdev.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "esp_vfs_fat_nand.h"   /* esp_vfs_fat_nand_bdl_format (optional) */
#include "driver/spi_master.h"
#include "soc/spi_pins.h"
#include "spi_nand_flash.h"
```

### 4. Initialization flow

Follow the same order as `example_spi_nand_setup()` / `app_main()` in the example source:

1. **SPI bus** — `spi_bus_initialize()` with `spi_bus_config_t` (MOSI, MISO, CLK, WP, HD GPIOs).
2. **SPI device** — `spi_bus_add_device()` with `spi_device_interface_config_t` (clock, CS, mode, `flags`).
3. **NAND driver** — `spi_nand_flash_init_with_layers()` with `spi_nand_flash_config_t` (returns WL `esp_blockdev_handle_t`).
4. **Optional pre-format** — `esp_vfs_fat_nand_bdl_format()` with `esp_vfs_fat_mount_config_t` (same config you will use for mount).
5. **FAT mount** — `esp_vfs_fat_bdl_mount()` with `esp_vfs_fat_mount_config_t`.
6. **File I/O** — standard `fopen()` / `fprintf()` / `fgets()` on paths under the mount point (e.g. `/nandflash/hello.txt`).
7. **Cleanup** — `esp_vfs_fat_bdl_unmount()`, then `wl_bdl->ops->release(wl_bdl)`, `spi_bus_remove_device()`, `spi_bus_free()` (see `example_spi_nand_teardown()`).

Default pin macros (`HOST_ID`, `PIN_MOSI`, etc.) are defined at the top of the example source. Wiring matches [examples/nand_flash](../nand_flash/README.md).

### 5. `spi_nand_flash_config_t`

Populated in `example_spi_nand_setup()` before `spi_nand_flash_init_with_layers()`:

| Field | Description |
|-------|-------------|
| `device_handle` | `spi_device_handle_t` returned by `spi_bus_add_device()` |
| `io_mode` | `SPI_NAND_IO_MODE_SIO` in this example; also `DIO`, `DOUT`, `QIO`, or `QOUT` |
| `flags` | `SPI_DEVICE_HALFDUPLEX` for half-duplex (required for DIO/DOUT/QIO/QOUT); `0` for full-duplex SIO. This value has to match the half-duplex flag in `spi_device_interface_config_t.flags` |
| `gc_factor` | Optional wear-leveling GC tuning; omit or set `0` for the driver default |

Full struct documentation is in [`spi_nand_flash.h`](../../../spi_nand_flash/include/spi_nand_flash.h). The same config type is used for legacy `spi_nand_flash_init_device()`.

### 6. Mount configuration

```c
const esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = false,  // or true to format on first mount failure
    // Cluster size used when formatting. Must be a power of 2 between the
    // logical sector size and 128 * sector size. Larger values improve
    // sequential throughput; smaller values waste less space for small files.
    // 16 KiB matches this example (and common ESP-IDF FatFs samples);
    // use 0 to default to one sector per cluster.
    .allocation_unit_size = 16 * 1024,
};

#ifdef CONFIG_EXAMPLE_FORMAT_BEFORE_MOUNT
ESP_ERROR_CHECK(esp_vfs_fat_nand_bdl_format(wl_bdl, &mount_config));
#endif
ESP_ERROR_CHECK(esp_vfs_fat_bdl_mount("/nandflash", wl_bdl, &mount_config));
```

In this example project, `format_if_mount_failed` and pre-mount format are controlled by `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` and `CONFIG_EXAMPLE_FORMAT_BEFORE_MOUNT` in menuconfig.

### 7. Prerequisites

- **ESP-IDF 6.1+** (`esp_vfs_fat_bdl_mount` / `diskio_bdl`).
- Enable **`CONFIG_NAND_FLASH_ENABLE_BDL=y`** (Component config → SPI NAND Flash). When BDL is on, **`spi_nand_flash_init_device()` is not available**.
- See also [`spi_nand_flash_fatfs` component README](../../README.md#requirements-read-first).

## Requirements

- **ESP-IDF 6.1 or newer** (`esp_vfs_fat_bdl_mount` / `diskio_bdl`; NAND BDL itself needs IDF 6.0+).
- **`CONFIG_NAND_FLASH_ENABLE_BDL=y`** (set in this example’s `sdkconfig.defaults`). When BDL is enabled, **`spi_nand_flash_init_device()` is not available**; use the layered API as shown here.

## Hardware

Same wiring as [examples/nand_flash](../nand_flash/README.md) (SPI bus + external SPI NAND).

## Configuration

- **`CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED`**: if enabled, the volume is formatted when mount fails (same idea as the legacy example).
- **`CONFIG_EXAMPLE_FORMAT_BEFORE_MOUNT`**: if enabled, the example calls **`esp_vfs_fat_nand_bdl_format()`** (from `spi_nand_flash_fatfs`) with the same **`esp_vfs_fat_mount_config_t`** (cluster size, one/two FATs) *before* **`esp_vfs_fat_bdl_mount()`**, so you can force a layout that matches your mount options without depending on mount-time formatting. CI enables this via `sdkconfig.ci`.
- **`sdkconfig.defaults`** sets **`CONFIG_ESP_TASK_WDT_EN=n`** so long format/mount work on NAND is less likely to hit the task watchdog.

## Build and run

```bash
cd examples/nand_flash_bdl
idf.py set-target esp32
idf.py -p PORT flash monitor
```

Expected log sequence: WL BDL geometry, optional “Pre-mount format finished”, mount, “Opening file” / “File written” / “Reading file” / “Read from file:”, unmount, “Done”.

## How this differs from `nand_flash`

| Example        | NAND init                         | FatFS mount                    | `CONFIG_NAND_FLASH_ENABLE_BDL` |
| -------------- | --------------------------------- | ------------------------------ | ------------------------------ |
| `nand_flash`   | `spi_nand_flash_init_device()`    | `esp_vfs_fat_nand_mount()`     | **Must be off**                |
| `nand_flash_bdl` | `spi_nand_flash_init_with_layers()` | `esp_vfs_fat_bdl_mount()`   | **Must be on** (this example)  |

Mount uses ESP-IDF’s **`esp_vfs_fat_bdl_mount()`**. Optional pre-format uses **`esp_vfs_fat_nand_bdl_format()`** from this component (same `f_mkfs` layout as IDF’s BDL mount path).
