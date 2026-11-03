| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash Test App

Unity tests for `spi_nand_flash` (legacy API, BDL, and generic chip detection).

## Sdkconfig CI presets

| File | Purpose |
|------|---------|
| `sdkconfig.ci.default` | Legacy API, generic detect **off** (baseline regression) |
| `sdkconfig.ci.bdl` | Block device layer, generic detect **off** |
| `sdkconfig.ci.generic` | BDL + `CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION=y` (Flash BDL bring-up path) |

CI builds all `sdkconfig.ci.*` files via the repo `idf_build_apps` matrix. On-target pytest (`pytest_spi_nand_flash.py`) runs on hardware for `default`, `bdl`, and `generic` when the `spi_nand_flash` runner is available.

## Generic detect — hardware validation

Default GitHub CI may **build** `sdkconfig.ci.generic` without running on-target tests if the hardware runner is unavailable. Before merging changes that touch generic/ONFI init:

1. Enable `CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION=y` (use `sdkconfig.ci.generic` or equivalent; requires BDL).
2. Confirm `nand_flash_get_blockdev()` succeeds, probe report has `page0_read_ok`, chip source is `ONFI` or `KCONFIG`.
3. Confirm `spi_nand_flash_init_with_layers()` returns `ESP_ERR_NOT_SUPPORTED`.
4. With write/erase Kconfig **off**, confirm Flash BDL `read_only` and write/erase return `ESP_ERR_NOT_SUPPORTED`.

**Ownership:** component maintainers or the engineer changing generic/ONFI code. If CI omits the on-target run, the PR must state that manual validation is pending or link completed evidence.

### Manual run (example)

```bash
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.generic" set-target esp32
idf.py build flash monitor
# or
pytest pytest_spi_nand_flash.py -k generic
```
