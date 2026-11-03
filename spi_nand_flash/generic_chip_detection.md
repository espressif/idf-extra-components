# Generic chip detection (opt-in, BDL only)

By default the driver only initializes parts in the supported list (**vendor database**). Optional **generic chip detection** is for board bring-up, unknown parts, and lab override of a known JEDEC ID. It is **disabled by default** and requires **`CONFIG_NAND_FLASH_ENABLE_BDL=y`**.

When `CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION=y`:

1. Read JEDEC ID, then **bypass the vendor database** (including chips that already have a DB entry).
2. Read ONFI parameter page (signature `ONFI` + CRC). Sets `chip_source` to `SPI_NAND_CHIP_SOURCE_ONFI`.
3. If the parameter page fails, use `CONFIG_NAND_FLASH_GENERIC_GEOMETRY_*` when all are non-zero (`SPI_NAND_CHIP_SOURCE_KCONFIG`); otherwise init fails.
4. Apply a conservative profile: **SIO**, **ECC STATUS not decoded**, no QE. Plane count comes from `CONFIG_NAND_FLASH_GENERIC_NUM_PLANES` (default **1**). Wrong plane count corrupts addressing — use the datasheet. On-die ECC: default path only **reads** CFG bit 4 into `on_die_ecc_enabled`; when `WRITE_ERASE` is enabled, SET FEATURE enables ECC and init **fails** if bit 4 is not confirmed.
5. Non-destructive probes: GET FEATURE (status) + page 0 read (no Write Enable).
6. Create **Flash BDL** via `nand_flash_get_blockdev()` — **read-only** unless `CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE=y`.
7. Wear-leveling BDL / `spi_nand_flash_init_with_layers()` / FatFS return **`ESP_ERR_NOT_SUPPORTED`**.

When the option is **off**, behavior is database-only (same as before).

## Kconfig

Component config → SPI NAND Flash configuration:

| Symbol | Default | Role |
|--------|---------|------|
| `CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION` | `n` | Master gate (depends on BDL) |
| `CONFIG_NAND_FLASH_GENERIC_GEOMETRY_*` | `0` | Parameter-page fallback (`0` = unset) |
| `CONFIG_NAND_FLASH_GENERIC_NUM_PLANES` | `1` | Plane count (1–4). Default single-plane; `>1` enables plane-select. Set from datasheet — incorrect values corrupt column addressing |
| `CONFIG_NAND_FLASH_GENERIC_T_*_US` | conservative | Timings for Kconfig geometry, and for an ONFI field that is 0. Non-zero ONFI timings are kept. Busy timeout is 10× this value |
| `CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE` | `n` | Allow Flash BDL program/erase (requires on-die ECC bit confirmed) |

## Public API

On the flash block device from `nand_flash_get_blockdev()`, ioctl `ESP_BLOCKDEV_CMD_GET_CHIP_SOURCE` returns `DATABASE`, `ONFI`, or `KCONFIG`. The wear-leveling block device does not implement that ioctl. For bring-up details (`param_page_valid`, `num_planes`, ECC), use `ESP_BLOCKDEV_CMD_GET_GENERIC_PROBE_REPORT` on the same flash block device (returns `ESP_ERR_NOT_SUPPORTED` when generic detection is off).

## Limitations (v1)

- ECC status is not decoded: reads that on-die ECC could not correct are still returned as success, and `ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS` reports `NAND_ECC_UNKNOWN`. `ESP_BLOCKDEV_CMD_GET_ECC_STATS` returns `ESP_ERR_NOT_SUPPORTED`.
- Page 0 probe success does not prove full-die mapping on multi-plane parts; configure `CONFIG_NAND_FLASH_GENERIC_NUM_PLANES` from the datasheet when the part is multi-plane.
- Fixed **4-byte** BBM/page-used markers at column `page_size`; ONFI spare size is log-only.
- Not a production FatFS / wear-leveling path — add a vendor DB entry for that.

**Production guidance:** Keep generic detection **off** and use a supported/list entry before shipping.
