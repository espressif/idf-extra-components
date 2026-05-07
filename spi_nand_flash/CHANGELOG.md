# Changelog

Versioning policy: see [VERSIONING.md](VERSIONING.md). From **v1.0.0** onward this component follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

## [1.0.3]
### Dependencies
- **Dhara** is now consumed as the in-repo `espressif/dhara` component at **1.0.0** (vendored upstream snapshot; the git submodule under `dhara/` is removed). The manifest dependency range is **`1.*`** (was `0.1.*`), matching the new component version with the same `override_path: "../dhara"` layout.

### Documentation
- README: clarify that Dhara is provided by the vendored `espressif/dhara` component and that no separate submodule checkout is required.

## [1.0.2]
### Fixes
- BDL error logging: correct format specifiers for size fields on 64-bit Linux (avoids undefined behavior and wrong log output).
- Linux NAND emulation: OOB markers and free-page detection aligned with the real hardware path.
- Linux mmap: correct backing file path selection for the emulated image.
- Linux mmap layout: on-disk stride accounts for interleaved OOB vs user-visible erase block size (bad-block handling, erase, and OOB clearing).

### Documentation
- Linux mmap emulator config: note how backing file size maps to interleaved OOB and reported user capacity.

### Testing
- Linux host tests: broader FTL and BDL coverage.
- Shared buffer pattern helpers (including seeded patterns) for host tests and the in-tree test application.

## [1.0.1]
- fix: fix incorrect flash geometry parameter for flash GD5F4GM8xExxG

## [1.0.0]
### Breaking Changes
- FATFS integration has been moved to a separate `spi_nand_flash_fatfs` component. Projects using FATFS with NAND flash must add `spi_nand_flash_fatfs` as a dependency. FatFs on SPI NAND still requires the **legacy** init path: keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled** for that use case (no FatFs-on-BDL support in this release).
- When `CONFIG_NAND_FLASH_ENABLE_BDL` is enabled, the legacy `spi_nand_flash_init_device()` returns `ESP_ERR_NOT_SUPPORTED`. Use `spi_nand_flash_init_with_layers()` instead for block-device consumers.
- `spi_nand_erase_chip()` performs a physical full-media erase. Previously, the driver incorrectly attempted to erase every block without checking bad-block markers, which could erase factory-marked bad blocks. It now skips bad blocks and physically erases only blocks that are not marked bad.

### New Features
- Added Block Device Layer (BDL) support, available from ESP-IDF v6.0. Provides standard `esp_blockdev_t` interfaces for both raw flash access and wear-leveling.
- Added `spi_nand_flash_init_with_layers()` API for layered block device initialization.
- Added page-based API terminology (`read_page`, `write_page`, `get_page_count`, `get_page_size`) with backward-compatible sector aliases.

### Improvements
- Refactor the component for improved structure, maintainability and readability
- Sector API functions are now deprecated aliases for the page API equivalents.

**Migration:** See **Migration Guide (0.x → 1.0.0)** in [layered_architecture.md](layered_architecture.md).

## [0.21.0]
- fix: spi_nand_read fails in case buffer is not DMA aligned (https://github.com/espressif/idf-extra-components/issues/708)

## [0.20.0]
- feat: added support for Gigadevice (GD5F1GM7xExxG) NAND flash

## [0.19.0]
- fix: spi_nand_program_load fails in case buffer is not DMA aligned (https://github.com/espressif/idf-extra-components/issues/684)

## [0.18.0]
- fix: Update esp_vfs_fat_register prototype to esp_vfs_fat_register_cfg to align with ESP-IDF v6.0. The cfg version is now the primary API and remains aliased for compatibility.

## [0.17.0] 
- fix: fix a compilation error caused by the missing freertos/FreeRTOS.h header when building with ESP-IDF v6.0 and later.
- update: improvements to the lower-level APIs following updates in esp_driver_spi.

## [0.16.0]
- fix: fix nand flash issue caused by data length unalignment on esp32p4

## [0.15.0]
- feat: added support for Gigadevice (GD5F2GM7xExxG) NAND flash

## [0.14.0]
- feat: added support for XTX (XT26G08D) and Gigadevice (GD5F4GM8) NAND flash

## [0.13.0]
- feat: added support for Zetta (ZD35Q1GC) NAND flash
        and Winbond (W25N02KVxxIR/U, W25N04KVxxIR/U) NAND flash chips.

## [0.12.0]
- feat: added micron dual-plane nand flash chip (MT29F2G) support
- fix: fixed build failure in host tests

## [0.11.0]
- feat: added QIO mode support

## [0.10.0]
- feat: added support for standard SPI mode (full-duplex) and DIO mode

## [0.9.0]
- feat: added linux target support
- fix: fixed memory alignment issue occurring on esp32c3

## [0.8.0]
- feat: added diagnostics application

## [0.7.0]
- feat: exposed lower-level API and make it usable without dhara library

## [0.6.0]
- feat: implemented CTRL_TRIM in fatfs diskio layer
- feat: added data refresh threshold for ECC correction

## [0.5.0]
- feat: added Kconfig option to verify write operations
- feat: added support for Micron MT29F1G01ABAFDSF-AAT:F

## [0.4.1]
- update: handled alignment and DMA requirements for work buffers

## [0.4.0]
- fix: fixed memory leaks in test, add performance log

## [0.3.1]
- fix: correct calloc call arguments for GCC14 compat

## [0.3.0]
- fix: use 32 bit sector size and id

## [0.2.0]
- feat: added support for Micron MT29F nand flash

## [0.1.0]
- Initial release with basic NAND flash support based on dhara nand library
- Support for Winbond, Alliance and Gigadevice devices
- Added test application for the same

