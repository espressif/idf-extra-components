## [1.0.0]

### Breaking Changes
- FATFS integration for SPI NAND Flash now lives in this component. Projects that previously relied on FATFS support bundled inside `spi_nand_flash` must add `spi_nand_flash_fatfs` as a dependency and include its headers.

**Migration:** See **Migration Guide (0.x → 1.0.0)** in [`spi_nand_flash/layered_architecture.md`](../spi_nand_flash/layered_architecture.md) (FATFS split, legacy init with BDL disabled, and related driver changes).
