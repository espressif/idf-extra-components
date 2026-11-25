# The slots backend

Partition pairs with a power-safe record of which slot is live.

This directory is part of [esp_suit_ota](../README.md), kept apart from the update engine. It
holds two files. `esp_ota_slots.c` owns the pairs and the record, and knows nothing about SUIT,
manifests or downloads. `esp_suit_ota_slots.c` is the adapter that hands those pairs to the update
engine, and it is the only file here that mentions SUIT.

An OTA update of the app has two app partitions and a boot selector. `esp_ota_slots.c` gives the
same treatment to any partition. You declare a logical name, the two partitions behind it, and the
kind of pair. It keeps one record in NVS that says which slot of each pair is live now, which slot
was live before, and whether the last change is settled.

Neither file is required. An application that leaves `cfg.slots_ops` at `NULL` maps every
component to a fixed partition instead, and the linker leaves both files out of the image.

## Kinds of pair

| Kind | Partitions | How a release is written |
| --- | --- | --- |
| `ESP_OTA_SLOTS_AB` | two | Written to the spare slot, then the record flips. Power safe. |
| `ESP_OTA_SLOTS_COPY` | two | Written to the staging partition, then copied into the live one. |
| `ESP_OTA_SLOTS_SINGLE` | one | Written in place. Not power safe and cannot go back. |
| `ESP_OTA_SLOTS_APP` | none | The OTA app partitions. `otadata` selects them, not this record. |

Name both partitions of a pair, or name neither. A pair that names neither derives the labels from
its own name: `<name>_0` and `<name>_1` for `AB`, `<name>` and `<name>_staging` for `COPY`, and
`<name>` for `SINGLE`.

Pairs that share a `group` flip together or not at all.

## Use it

```c
static const esp_ota_slots_decl_t slots[] = {
    { .name = "app", .kind = ESP_OTA_SLOTS_APP, .group = "cal" },
    { .name = "f1",  .kind = ESP_OTA_SLOTS_AB,  .primary = "f1_live", .secondary = "f1_spare", .group = "cal" },
};

nvs_flash_init();
esp_ota_slots_init(slots, 2);               /* call this before any read of the data */
const esp_partition_t *live = esp_ota_slots_get_active("f1");
```

`esp_ota_slots_init()` also settles the last release. If the record is `ACTIVATING`, power was lost
inside an activation, and every pair goes back to its previous slot. If the record is `PENDING` for
an app slot that is not running, the bootloader rolled the app back, so the data that came with it
goes back too.

## The record states

```
CONFIRMED  --commit_flip()-->  ACTIVATING  --activation_done()-->  PENDING  --confirm()-->  CONFIRMED
                                    |                                 |
                                    +------- init() after a power cut +--revert()--> CONFIRMED (previous slots)
```

After the release is live, the application decides. Call `esp_ota_slots_confirm()` to keep it. Call
`esp_ota_slots_revert()` to send every pair back. Call `esp_ota_slots_revert_group("f1")` to send
one pair and the rest of its group back, and leave the other pairs live.

## With the update engine

Call `esp_ota_slots_init()` with your declarations, then put `esp_suit_ota_slots_ops()` in
`esp_suit_ota_config_t.slots_ops`. Nothing is added to `idf_component.yml`, because both files
come with `esp_suit_ota`.

```c
#include "esp_ota_slots.h"
#include "esp_suit_ota_slots.h"

nvs_flash_init();
esp_ota_slots_init(slots, 2);
cfg.slots_ops = esp_suit_ota_slots_ops();
```
