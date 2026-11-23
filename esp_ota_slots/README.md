# esp_ota_slots

[![Component Registry](https://components.espressif.com/components/espressif/esp_ota_slots/badge.svg)](https://components.espressif.com/components/espressif/esp_ota_slots)

Partition pairs with a power-safe record of which slot is live.

An OTA update of the app has two app partitions and a boot selector. This component gives the same
treatment to any partition. You declare a logical name, the two partitions behind it, and the kind
of pair. The component keeps one record in NVS that says which slot of each pair is live now, which
slot was live before, and whether the last change is settled.

The component knows nothing about SUIT, about manifests or about downloads. It only owns the
record and the flip.

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

## With esp_suit_ota

The `esp_suit_ota_slots` component turns these pairs into the slots backend of `esp_suit_ota`. Add
it, call `esp_ota_slots_init()`, and put `esp_suit_ota_slots_ops()` in
`esp_suit_ota_config_t.slots_ops`.
