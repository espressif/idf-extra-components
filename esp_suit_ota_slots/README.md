# esp_suit_ota_slots

[![Component Registry](https://components.espressif.com/components/espressif/esp_suit_ota_slots/badge.svg)](https://components.espressif.com/components/espressif/esp_suit_ota_slots)

The adapter that lets [esp_suit_ota](../esp_suit_ota) use the partition pairs of
[esp_ota_slots](../esp_ota_slots).

`esp_suit_ota` owns the manifest, the signature check, the downloads and the digests. It knows
nothing about slots. It asks a backend for the partitions of a named target, and at the end of
`esp_suit_ota_activate()` it asks that backend to record the new slots.

`esp_ota_slots` owns the pairs and the record. It knows nothing about SUIT.

This component is the one file that joins them. It is separate so that neither of the two depends
on the other, and so that an application can use either one on its own.

## Use it

```c
#include "esp_ota_slots.h"
#include "esp_suit_ota_slots.h"

static const esp_ota_slots_decl_t slots[] = {
    { .name = "app", .kind = ESP_OTA_SLOTS_APP },
    { .name = "f1",  .kind = ESP_OTA_SLOTS_AB },     /* f1_0 and f1_1 */
};

nvs_flash_init();
esp_ota_slots_init(slots, 2);

esp_suit_ota_config_t cfg = { /* keys, vendor and class identifiers */ };
cfg.slots_ops = esp_suit_ota_slots_ops();
```

With `cfg.map` left at `NULL`, the last part of each component identifier names a pair. The
identifier `[esp32c5, f1]` reaches the pair `f1`, and `[esp32c5, app]` reaches the app pair.

## Without this component

`esp_suit_ota` still works with `cfg.slots_ops` set to `NULL`. Every component then needs a map
entry that names a fixed partition or a custom sink, and the download is written straight into it.
There are no A/B slots, and a data partition has no previous copy to return to. The app still rolls
back, because the bootloader owns that decision.
