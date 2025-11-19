# ESP External Partition Tables

This component provides an API to parse and generate external partition tables.

Currently only [MBR (Master boot record)](https://en.wikipedia.org/wiki/Master_boot_record) is supported.

## Features

- Parse MBR partition tables from raw data (e.g., SD card, USB drive)
- Generate and manipulate partition lists in memory
- Deep copy and de-initialize partition lists
- Access partition information (address, size, type, label)
- Example projects included

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
    printf("Partition %d:\n
        address: %" PRIu64 "\n
        size: %" PRIu64 "\n
        type: %" PRIu32 "\n,
        label: %s\n",
    i, item->info.address, item->info.size, (uint32_t) item->info.type, item->label ? item->label : ""); // item->info.type is of `esp_ext_part_type_known_t` enum type

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
