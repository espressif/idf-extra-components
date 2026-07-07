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

- `spi_nand_flash` component (driver; pulled in automatically)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

This component provides VFS mount helpers (`esp_vfs_fat_nand_mount()`, `esp_vfs_fat_nand_unmount()` in `esp_vfs_fat_nand.h`) on top of a `spi_nand_flash_device_t` handle from `spi_nand_flash_init_device()`.

Your application must initialize the SPI bus and NAND driver before calling the mount helpers. **For the full end-to-end flow** — dependency, CMake, headers, SPI initialization, `spi_nand_flash_config_t`, FAT mount, and file I/O — see:

- [`examples/nand_flash`](examples/nand_flash) — reference source: [`main/spi_nand_flash_example_main.c`](examples/nand_flash/main/spi_nand_flash_example_main.c)
- [`examples/nand_flash/README.md`](examples/nand_flash/README.md) — integration guide and hardware notes

## Examples

| Example | Description | `CONFIG_NAND_FLASH_ENABLE_BDL` | IDF |
|---------|-------------|--------------------------------|-----|
| `examples/nand_flash` | FATFS on NAND (`spi_nand_flash_init_device` + `esp_vfs_fat_nand_mount`) | **Must be off** | 5.0+ |
| `examples/nand_flash_debug_app` | Diagnostics (bad blocks, ECC stats, throughput); `spi_nand_flash` only, no VFS | **Must be off** | 5.0+ |

See each example’s `README.md` for hardware and usage.
