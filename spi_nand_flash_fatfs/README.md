# SPI NAND Flash FatFS Integration

FatFS integration layer for the SPI NAND Flash driver.

## Requirements (read first)

**`spi_nand_flash_fatfs` only supports the legacy driver handle** (`spi_nand_flash_device_t` from `spi_nand_flash_init_device()`).

- Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled** in menuconfig. When BDL is enabled, `spi_nand_flash_init_device()` returns `ESP_ERR_NOT_SUPPORTED`, so `esp_vfs_fat_nand_mount()` / diskio cannot be used.
- **FatFs on top of the wear-leveling block device (`esp_blockdev_t`) is not supported in this release.** A future component integration will add it; until then use the legacy path above for FAT.

**Migration from 0.x:** See the SPI NAND component’s [layered_architecture.md](../spi_nand_flash/layered_architecture.md) — **Migration Guide (0.x → 1.0.0)** (FATFS split, BDL vs legacy init).

## Features

- FATFS diskio adapter and VFS mount helpers using `spi_nand_flash_device_t`
- Same usage as before the `spi_nand_flash` / `spi_nand_flash_fatfs` split (legacy init + mount)

## Dependencies

- `spi_nand_flash` component (driver)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

```c
#include "spi_nand_flash.h"
#include "esp_vfs_fat_nand.h"

// Initialize device (CONFIG_NAND_FLASH_ENABLE_BDL must be off)
spi_nand_flash_device_t *nand_device;
spi_nand_flash_init_device(&config, &nand_device);

// Mount FATFS
esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
};
esp_vfs_fat_nand_mount("/nand", nand_device, &mount_config);

// Use filesystem...
FILE *f = fopen("/nand/test.txt", "w");
// ...

// Unmount
esp_vfs_fat_nand_unmount("/nand", nand_device);
spi_nand_flash_deinit_device(nand_device);
```

## Examples

| Example | Description | `CONFIG_NAND_FLASH_ENABLE_BDL` | IDF |
|---------|-------------|--------------------------------|-----|
| `examples/nand_flash` | FATFS on NAND (`spi_nand_flash_init_device` + `esp_vfs_fat_nand_mount`) | **Must be off** | 5.0+ |
| `examples/nand_flash_debug_app` | Diagnostics (bad blocks, ECC stats, throughput); `spi_nand_flash` only, no VFS | **Must be off** | 5.0+ |

See each example’s `README.md` for hardware and usage.
