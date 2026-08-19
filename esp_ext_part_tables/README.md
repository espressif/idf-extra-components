# ESP External Partition Tables

This component provides an API to parse and generate external partition tables.

Currently only [MBR (Master boot record)](https://en.wikipedia.org/wiki/Master_boot_record) is supported.

## Features

- Parse MBR partition tables from raw data (e.g., SD card, USB drive)
- Generate and manipulate partition lists in memory
- Deep copy and de-initialize partition lists
- Access partition information (address, size, type, label)
- Filter partitions with a caller predicate (e.g. only mountable filesystems)
- Example projects included

## Partition selection and filtering (MBR parsing)

`esp_mbr_parse` inserts **every recognized partition** into the list by default,
regardless of whether ESP-IDF can mount it (FAT12/16/32, LittleFS, `0xDA` raw data,
and also recognized-but-undrivable types such as exFAT/NTFS, Linux and
GPT-protective MBR). Only truly unknown/extended type codes are skipped.

To select a subset, iterate with a predicate via `esp_ext_part_list_next_matching`,
or filter at parse time with `esp_mbr_parse_extra_args_t.match`.

The usage classes are a coarse, *type-intrinsic* hint. Whether a filesystem is
actually **mountable in a given firmware** depends on which drivers are linked in
that build (for example LittleFS is an optional external component), which the
library cannot decide on its own. So mountability is expressed as a predicate.

The library ships a ready-made matcher, `esp_ext_part_match_mountable()`, so you
usually do not need to write your own. It treats FAT12/16/32 as always mountable
(FatFs is part of ESP-IDF) and LittleFS as mountable when the LittleFS component is
visible at compile time (detected via `__has_include("esp_littlefs.h")`, or forced by
defining `ESP_EXT_PART_HAS_LITTLEFS`):

```c
esp_ext_part_match_t matcher = esp_ext_part_match_mountable();
for (esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, &part_list, &matcher);
     it != NULL;
     it = esp_ext_part_list_next_matching(it, &part_list, &matcher)) {
    // it points to a partition this build can mount
}
```

To decide differently - e.g. gate on a specific Kconfig option, or branch on a
runtime field such as a LittleFS block size stored in `info->extra` - supply your own
predicate. It receives the full partition info plus an opaque context:

```c
static bool i_can_mount(const esp_ext_part_t *info, void *ctx)
{
    switch (info->type) {
    case ESP_EXT_PART_TYPE_FAT12:
    case ESP_EXT_PART_TYPE_FAT16:
    case ESP_EXT_PART_TYPE_FAT32:
        return true;              // FatFs is part of ESP-IDF
#ifdef CONFIG_LITTLEFS_PAGE_SIZE // only if THIS build links the LittleFS component
    case ESP_EXT_PART_TYPE_LITTLEFS:
        return true;
#endif
    default:
        return false;
    }
}

// esp_ext_part_match_t matcher = { .fn = i_can_mount };
// ... esp_ext_part_list_next_matching(NULL, &part_list, &matcher) ...
```

The same predicate can filter **at parse time** via `esp_mbr_parse_extra_args_t`, so
unwanted partitions are never inserted:

```c
esp_mbr_parse_extra_args_t args = { .match = esp_ext_part_match_mountable() };
esp_mbr_parse((void*) loaded_mbr, &part_list, &args); // only mountable partitions inserted
```

Whenever the parser skips a partition (an unknown/extended type, or one rejected by
`match`), it sets `ESP_EXT_PART_LIST_FLAG_LOSSY` on the list. When that flag is
**unset**, every partition on the source medium was captured, so regenerating an MBR
from the list is functionally equivalent to the source (ignoring cosmetic differences
such as CHS values or the disk signature).

## Alignment and layout validation (MBR generation)

When generating an MBR (`esp_mbr_generate` / `esp_ext_part_list_bdl_write`), the
behavior is controlled through `esp_mbr_generate_extra_args_t` (a zero-initialized
struct selects all defaults):

The sector size used for all byte<->sector math comes from the partition list's
`sector_size` field (set by `esp_mbr_parse`, or assigned directly on a freshly
built list, e.g. `part_list.sector_size = ESP_EXT_PART_SECTOR_SIZE_4KiB;`). It can
be overridden per call via `extra_args.sector_size`; if neither is set it defaults
to 512 B.

- `total_size`: when non-zero, partitions that extend past this many bytes are
  rejected. The block-device write helper auto-fills this from the device geometry
  when left `0`.
- `alignment`: partition start alignment. `ESP_EXT_PART_ALIGN_AUTO` (the value a
  zero-initialized struct selects) resolves to a 1 MiB default;
  `ESP_EXT_PART_ALIGN_NONE` leaves start LBAs untouched; `ESP_EXT_PART_ALIGN_4KiB`
  and `ESP_EXT_PART_ALIGN_1MiB` request a specific alignment.
- `align_policy`: what happens to a partition's size when alignment moves its
  start. `ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE` (default) keeps the requested size
  as the length from the aligned start (matching `fdisk`/`parted`);
  `ESP_EXT_PART_ALIGN_POLICY_REJECT` returns an error if a start was not already
  aligned; `ESP_EXT_PART_ALIGN_POLICY_PRESERVE_END` shrinks the size so the end
  stays at the originally requested `address + size`.

Overlapping partitions are always rejected, and a list item with type
`ESP_EXT_PART_TYPE_NONE` (which would create a gap that truncates the parsed
table) is rejected with `ESP_ERR_INVALID_ARG`.

## Automatic partition placement (MBR generation)

Instead of computing every start address by hand, a partition can be placed
automatically by setting flags on `esp_ext_part_t.flags`:

- `ESP_EXT_PART_FLAG_AUTO_ADDRESS`: the library computes the partition's start,
  placing it right after the previous partition and aligning it. `info.address` is
  ignored. The first auto-placed partition lands on the first aligned LBA (after
  the MBR sector).
- `ESP_EXT_PART_FLAG_FILL` (with `AUTO_ADDRESS` and `info.size == 0`): the
  partition is sized to fill from its computed start to the end of the disk. This
  needs a known disk size - either `extra_args->total_size`, or (via
  `esp_ext_part_list_bdl_write`) the block device geometry.

The caller's partition list is never modified; addresses/sizes are resolved into
internal copies during generation.

```c
// First partition: auto-placed, fixed size. Last partition: auto-placed, fills the rest.
esp_ext_part_list_item_t p0 = {
    .info = {
        .size = 16 * 1024 * 1024, // 16 MiB
        .type = ESP_EXT_PART_TYPE_FAT32,
        .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
    }
};
esp_ext_part_list_item_t p1 = {
    .info = {
        .size = 0, // filled to the end of the disk
        .type = ESP_EXT_PART_TYPE_LITTLEFS,
        .extra = 4096,
        .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS | ESP_EXT_PART_FLAG_FILL | ESP_EXT_PART_FLAG_EXTRA,
    }
};
// esp_ext_part_list_insert(&part_list, &p0/&p1); then esp_ext_part_list_bdl_write(...)
```

## Example code

```c
// loaded_mbr -> Pointer to an array of 512 bytes containing MBR loaded from somewhere (SD card, etc.)

#include <stdio.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_ext_part_tables.h"
#include "esp_mbr.h"

esp_err_t err = ESP_OK;
esp_ext_part_list_t part_list = {0};

err = esp_mbr_parse((void*) loaded_mbr, &part_list, NULL); // Parse the array containing MBR and fill `esp_ext_part_list_t part_list` structure
if (err != ESP_OK) {
    return err;
}

esp_ext_part_list_item_t* item;
item = esp_ext_part_list_item_head(&part_list); // Get the first partition

for (int i = 0; item != NULL; i++) {
    printf("Partition %d:\n\taddress: %" PRIu64 "\n\tsize: %" PRIu64 "\n\ttype: %" PRIu32 "\n\tlabel: %s\n",
           i, item->info.address, item->info.size, (uint32_t) item->info.type,
           item->info.label ? item->info.label : ""); // item->info.type is of `esp_ext_part_type_known_t` enum type

    item = esp_ext_part_list_item_next(item); // Get the next partition
}

// ...

// Clean up when done
esp_ext_part_list_deinit(&part_list);
```

## More Examples

Runnable example projects can be found in [`examples/`](/esp_ext_part_tables/examples/) folder.

## API Reference

See [`esp_ext_part_tables.h`](/esp_ext_part_tables/include/esp_ext_part_tables.h) for the full API documentation.

More advanced API documentation can be found here: [`esp_mbr.h`](/esp_ext_part_tables/include/esp_mbr.h), [`esp_mbr_utils.h`](/esp_ext_part_tables/include/esp_mbr_utils.h).
