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

