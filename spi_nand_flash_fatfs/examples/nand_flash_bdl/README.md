| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash — FatFS via BDL (Block Device Layer)

This example is like [examples/nand_flash](../nand_flash/README.md), but it mounts FatFS using ESP-IDF’s generic BDL path:

- **`spi_nand_flash_init_with_layers()`** builds **Flash BDL → WL BDL** (Dhara wear leveling).
- **`esp_vfs_fat_bdl_mount()`** / **`esp_vfs_fat_bdl_unmount()`** connect VFS + FatFS to that WL block device.

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
