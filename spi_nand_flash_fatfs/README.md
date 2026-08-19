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

- `spi_nand_flash` component (driver; pulled in automatically)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

Pick **one** path from [Requirements](#requirements-read-first). Both need SPI bus setup and a filled **`spi_nand_flash_config_t`** before NAND init.

**Full end-to-end guides** (dependency, CMake, headers, SPI init, config fields, mount, file I/O) live in the example READMEs — those are the canonical references:

### Legacy mode (`esp_vfs_fat_nand_*`)

- Mount helpers: **`esp_vfs_fat_nand_mount()`** / **`esp_vfs_fat_nand_unmount()`** in `esp_vfs_fat_nand.h`, on a handle from **`spi_nand_flash_init_device()`**
- **`CONFIG_NAND_FLASH_ENABLE_BDL` must be off**
- Integration guide: [`examples/nand_flash/README.md`](examples/nand_flash/README.md)
- Source: [`examples/nand_flash/main/spi_nand_flash_example_main.c`](examples/nand_flash/main/spi_nand_flash_example_main.c)

### BDL mode (`esp_vfs_fat_bdl_*`)

- Init: **`spi_nand_flash_init_with_layers()`** → wear-leveling **`esp_blockdev_t`**
- Mount: ESP-IDF **`esp_vfs_fat_bdl_mount()`** / **`esp_vfs_fat_bdl_unmount()`**
- Optional pre-format: **`esp_vfs_fat_nand_bdl_format()`** (this component) with the same **`esp_vfs_fat_mount_config_t`**
- **`CONFIG_NAND_FLASH_ENABLE_BDL=y`**, ESP-IDF **6.1+**
- Integration guide: [`examples/nand_flash_bdl/README.md`](examples/nand_flash_bdl/README.md)
- Source: [`examples/nand_flash_bdl/main/spi_nand_flash_bdl_example_main.c`](examples/nand_flash_bdl/main/spi_nand_flash_bdl_example_main.c)

## Examples (this component)

All paths are under **`spi_nand_flash_fatfs/examples/`**:

| Example | Mode | `CONFIG_NAND_FLASH_ENABLE_BDL` | IDF |
|---------|------|--------------------------------|-----|
| `nand_flash` | Legacy FatFS | **Off** | 5.0+ |
| `nand_flash_bdl` | BDL + `esp_vfs_fat_bdl_*` | **On** | 6.1+ |
| `nand_flash_debug_app` | Diagnostics only (no VFS) | **Off** | 5.0+ |

See each example’s `README.md` for wiring, menuconfig, and **Use in your own project**.
