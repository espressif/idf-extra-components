# ESP External Partition Tables

This component provides an API to parse and generate external partition tables.

Currently only [MBR (Master boot record)](https://en.wikipedia.org/wiki/Master_boot_record) is supported.

## Features

- Parse MBR partition tables from raw data (e.g., SD card, USB drive)
- Generate and manipulate partition lists in memory
- Deep copy and de-initialize partition lists
- Access partition information (address, size, type, label)
- Example projects included

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
