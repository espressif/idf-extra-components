# esp_nand_ubi

A UBI-like Block Device Layer (BDL) middleware for SPI NAND flash. It wraps a raw
NAND flash BDL, hides bad blocks, provides logical erase blocks (LEBs), and enables
position-independent factory images: the filesystem above only ever sees LEB numbers,
never physical PEB numbers.

## Why this component

`esp_partition` was designed for NOR flash — fixed physical offsets, no bad blocks —
and cannot sit below a filesystem on raw NAND without breaking position-independence
at the partition-table level. This component replaces that role for NAND. UBI volumes
are the partitioning scheme for NAND.

## Stack

```
SPI NAND hardware
    |
nand_flash_get_blockdev(&spi_cfg, &nand_bdl)        [spi_nand_flash]
    |
nand_ubi_attach(nand_bdl, &cfg, &ubi_dev)           [esp_nand_ubi — scan once]
nand_ubi_open_volume(ubi_dev, 0, &vol_bdl)          [whole chip = one volume]
    |   geometry: disk_size = leb_count x LEB_SIZE, erase_size = LEB_SIZE
    |   bad blocks invisible; no physical addresses above this point
LittleFS / FatFS via vol_bdl
```

> The handle passed to `nand_ubi_attach()` must be the **raw** flash BDL from
> `nand_flash_get_blockdev()`, not the Dhara wear-leveling BDL from
> `spi_nand_flash_wl_get_blockdev()`. Stacking UBI on top of Dhara produces a
> double FTL with incompatible geometry contracts and double write amplification.

## Requirements

- ESP-IDF >= 6.0
- `CONFIG_NAND_FLASH_ENABLE_BDL` enabled in the `spi_nand_flash` component
- `CONFIG_ESP_NAND_UBI_ENABLE` enabled in this component

## Usage

Single-volume common case (whole NAND is one filesystem):

```c
#include "esp_nand_ubi.h"
#include "esp_nand_blockdev.h"

esp_blockdev_handle_t nand_bdl = NULL;
ESP_ERROR_CHECK(nand_flash_get_blockdev(&spi_cfg, &nand_bdl));

esp_blockdev_handle_t vol_bdl = NULL;
nand_ubi_config_t cfg = NAND_UBI_CONFIG_DEFAULT();
ESP_ERROR_CHECK(nand_ubi_get_blockdev(nand_bdl, &cfg, &vol_bdl));

/* mount a filesystem on vol_bdl ... */

vol_bdl->ops->release(vol_bdl);   /* also detaches the UBI device */
nand_bdl->ops->release(nand_bdl);
```

Multi-step (explicit device and volume lifecycle):

```c
nand_ubi_device_t *ubi_dev = NULL;
ESP_ERROR_CHECK(nand_ubi_attach(nand_bdl, &cfg, &ubi_dev));

esp_blockdev_handle_t vol_bdl = NULL;
ESP_ERROR_CHECK(nand_ubi_open_volume(ubi_dev, 0, &vol_bdl));

/* ... use vol_bdl ... */

vol_bdl->ops->release(vol_bdl);
ESP_ERROR_CHECK(nand_ubi_detach(ubi_dev));
nand_bdl->ops->release(nand_bdl);
```

## On-flash format

Two 64-byte headers per physical erase block (PEB), both at the start of the block,
byte-compatible with Linux UBI EC/VID headers:

```
PEB offset 0            EC header  (magic "UBI#")  image_seq, ec, offsets, CRC
vid_hdr_offset          VID header (magic "UBI!")  vol_id=0, lnum, sqnum, CRC
data_offset             LEB data   (LEB_SIZE = PEB_SIZE - data_offset)
```

With `data_offset = 2 x page_size`, images built with `ubinize` from `mtd-utils`
are compatible with this layer. The `esp_ubinize.py` host tool (Phase 2) is a simpler
alternative for users without `mtd-utils`.

## Status

Phase 1 (minimum viable layer): attach scan, EBA table, per-volume read/write/erase,
passive bad-block hiding. Wear-leveling, fastmap, and multi-volume support are planned
for later phases.
