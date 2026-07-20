# SPI NAND Flash FatFS Integration

FatFS-related examples and helpers for the SPI NAND Flash driver (`spi_nand_flash`).

## Requirements (read first)

Choose **one** integration path; they use different init APIs and Kconfig:

1. **Legacy mode** — ESP-IDF **5.0+**. Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled**. Use **`spi_nand_flash_init_device()`**, then this component’s **`esp_vfs_fat_nand_mount()`** / **`esp_vfs_fat_nand_unmount()`** (custom NAND diskio). Example: **`examples/nand_flash`**.

2. **BDL + FatFS mode** — ESP-IDF **6.1+** only (`esp_vfs_fat_bdl_*` / `diskio_bdl`). Enable **`CONFIG_NAND_FLASH_ENABLE_BDL=y`**. Use **`spi_nand_flash_init_with_layers()`**, then ESP-IDF’s **`esp_vfs_fat_bdl_mount()`** / **`esp_vfs_fat_bdl_unmount()`**. Optional mandatory pre-format: **`esp_vfs_fat_nand_bdl_format()`** in this component. Example: **`examples/nand_flash_bdl`**.

**Migration from 0.x:** See [`spi_nand_flash/layered_architecture.md`](../spi_nand_flash/layered_architecture.md) — **Migration Guide (0.x → 1.0.0)** (FatFS split, legacy vs BDL).

## Features

- **Legacy:** diskio adapter and VFS helpers (`esp_vfs_fat_nand.h`) for **`spi_nand_flash_device_t`**.
- **BDL:** **`esp_vfs_fat_nand_bdl_format()`** for optional pre-mount format; mount with IDF **`esp_vfs_fat_bdl_*`** (see **`examples/nand_flash_bdl`**).

## Dependencies

- `spi_nand_flash` component (driver)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

### Legacy mode (`esp_vfs_fat_nand_*`)

```c
#include "spi_nand_flash.h"
#include "esp_vfs_fat_nand.h"

// CONFIG_NAND_FLASH_ENABLE_BDL must be off
spi_nand_flash_device_t *nand_device;
spi_nand_flash_init_device(&config, &nand_device);

esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
};
esp_vfs_fat_nand_mount("/nand", nand_device, &mount_config);

FILE *f = fopen("/nand/test.txt", "w");
// ...
esp_vfs_fat_nand_unmount("/nand", nand_device);
spi_nand_flash_deinit_device(nand_device);
```

### BDL mode (`esp_vfs_fat_bdl_*`)

Use the wear-leveling block device from **`spi_nand_flash_init_with_layers()`** with **`esp_vfs_fat_bdl_mount()`**. To format unconditionally before mount (e.g. chosen **`allocation_unit_size`**), call **`esp_vfs_fat_nand_bdl_format()`** with the same **`esp_vfs_fat_mount_config_t`**, then mount. Details: **`examples/nand_flash_bdl/README.md`**.

## Examples (this component)

All paths are under **`spi_nand_flash_fatfs/examples/`**:

| Example | Mode | `CONFIG_NAND_FLASH_ENABLE_BDL` | IDF |
|---------|------|--------------------------------|-----|
| `nand_flash` | Legacy FatFS | **Off** | 5.0+ |
| `nand_flash_bdl` | BDL + `esp_vfs_fat_bdl_*` | **On** | 6.1+ |
| `nand_flash_debug_app` | Diagnostics only (no VFS) | **Off** | 5.0+ |

See each example’s `README.md` for wiring and menuconfig.
