# esp_suit_ota

[![Component Registry](https://components.espressif.com/components/espressif/esp_suit_ota/badge.svg)](https://components.espressif.com/components/espressif/esp_suit_ota)

SUIT manifest based OTA for ESP-IDF. One signed manifest describes a whole release: the app, data partitions, or both. The device verifies the manifest before it downloads a payload byte. It writes every payload to a passive slot, verifies every payload, and then switches the whole set at once. A release applies completely or not at all.

The component implements [draft-ietf-suit-manifest-38](https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/) for the updater role. The draft is in the RFC Editor queue. Payloads go through the native `app_update` API (`esp_ota_begin`, `esp_ota_write`, `esp_ota_end`). App images therefore get the same checks as with `esp_https_ota`: Secure Boot signature, anti-rollback, chip ID and chip revision.

Supported: ESP-IDF 6.0 and later. Tested on ESP32-C5 and ESP32-P4.

## How a release works

```
release.yml        build/app.bin     f1.bin
     │                   │             │
     └──── esp-suit create ────────────┘
                   │
                   ▼
   release.suit (signed manifest, about 500 bytes)  +  app.bin, f1.bin on the server
```

The device fetches `release.suit`. It verifies the COSE signature, the vendor and class identifiers and the sequence number. Then it runs the manifest and fetches only the components that are not already installed.

## Install

```
idf.py add-dependency "espressif/esp_suit_ota"
pip install -e path/to/esp_suit_ota/tools        # the esp-suit command
```

The `esp-suit` tool needs Python 3.9 or later, with `cbor2`, `cryptography` 44 or later, and `PyYAML`. You can also run it without installing: `python tools/esp_suit.py <command>`. See [tools/README.md](tools/README.md).

## Quick start

1. Make a partition table with an OTA app pair and one or more data pairs.

   ```
   # Name,     Type, SubType,   Offset,   Size
   nvs,        data, nvs,       0x9000,   24K
   otadata,    data, ota,       0xf000,   8K
   phy_init,   data, phy,       0x11000,  4K
   ota_0,      app,  ota_0,     0x20000,  1536K
   ota_1,      app,  ota_1,     ,         1536K
   f1_0,       data, 0x41,      ,         256K
   f1_1,       data, 0x41,      ,         256K
   ```

   These labels are the default labels of an AB pair named `f1`. Other labels work if the pair declaration names them.

2. Make the signing key and the device identity. Do this once per product.

   ```
   esp-suit keygen keys/manifest_key.pem --anchor main/keys/anchor_p256.bin
   esp-suit ids --vendor example.com --class my-product-hw2 -o main/suit_identity.h
   ```

   Keep `manifest_key.pem` secret. Embed `anchor_p256.bin` in the firmware with `EMBED_FILES`.

3. Add the boot code. It must run before any other code reads the data partitions.

   ```c
   static const esp_ota_slots_decl_t slots[] = {
       { .name = "app", .kind = ESP_OTA_SLOTS_APP },
       { .name = "f1",  .kind = ESP_OTA_SLOTS_AB },     /* f1_0 and f1_1 */
   };
   nvs_flash_init();
   esp_ota_slots_init(slots, 2);
   esp_suit_ota_recover();                       /* settle a campaign cut by power loss */
   if (self_test_ok()) {
       esp_suit_ota_confirm();                   /* cancel rollback, settle the slots record */
   } else {
       esp_ota_mark_app_invalid_rollback_and_reboot();
   }
   const esp_partition_t *f1 = esp_ota_slots_get_active("f1");
   ```

   Set `CONFIG_BOOTLOADER_APP_ROLLBACK_CONFIRM_BY_APP=y`. In the default mode, the bootloader confirms the new app before `app_main()` runs, and your self-test cannot stop a bad app.

4. Run the update. The application owns the loop, the buffer and the task.

   ```c
   static const esp_suit_ota_key_t keys[] = { { .point = anchor, .point_len = 65 } };
   static const esp_suit_ota_config_t cfg = {
       .keys = keys, .num_keys = 1,
       .vendor_ids = &vendor_uuid, .num_vendor_ids = 1,
       .class_ids = &class_uuid, .num_class_ids = 1,
   };
   static esp_http_client_config_t http = { .crt_bundle_attach = esp_crt_bundle_attach };
   static esp_suit_ota_https_cfg_t https = { .http_config = &http };

   esp_err_t update(const char *url, bool *restart)
   {
       esp_suit_ota_transport_t t = {
           .https = { .get = esp_suit_ota_https_get, .close = esp_suit_ota_https_close, .cfg = &https },
           .url = url,
       };
       static uint8_t buf[4096];
       esp_suit_ota_handle_t h;
       esp_err_t err = esp_suit_ota_begin(&cfg, &t, &h);
       if (err != ESP_OK) {
           return err;
       }
       for (;;) {
           int n = esp_suit_ota_transport_get(&t, buf, sizeof(buf));
           if (n < 0) {
               err = (esp_err_t)(-n);
               break;
           }
           size_t off = 0;
           do {                                  /* feed() stops where the next resource begins */
               size_t used = 0;
               err = esp_suit_ota_feed(h, buf + off, (size_t)n - off, &used);
               off += used;
           } while (err == ESP_OK && off < (size_t)n);
           if (err != ESP_OK || (n == 0 && t.done)) {
               break;
           }
       }
       if (err == ESP_SUIT_OTA_READY) {
           err = esp_suit_ota_commit(h);         /* read every staged image back and check it */
           if (err == ESP_OK) {
               err = esp_suit_ota_activate(h, restart);
           }
       }
       esp_suit_ota_end(h);                      /* keeps the journal, so a cut run resumes */
       return err;
   }
   ```

   `ESP_ERR_SUIT_OTA_UP_TO_DATE` and `ESP_ERR_SUIT_OTA_NOT_MODIFIED` mean that there is nothing to do. If `restart` is true, restart the device. The self-test in step 3 then decides about the new app. If `restart` is false, the release changed only data and the new data is live now. Make sure that the data is correct, then call `esp_suit_ota_confirm()` or `esp_suit_ota_revert()`.

5. Make the release on the build machine.

   ```yaml
   # release.yml
   sequence_number: auto-utc
   vendor: example.com
   class: my-product-hw2
   text: { en: { update-description: "1.2.3: fixes" } }
   components:
     - { id: [board, app], file: build/app.bin, uri: app.bin, skip_if_installed: true }
     - { id: [board, f1],  file: f1.bin,        uri: f1.bin }
   signing:
     keys: [keys/manifest_key.pem]
   ```

   ```
   esp-suit create release.yml --build-dir build
   esp-suit verify release.suit --key main/keys/anchor_p256.bin --payload app.bin=build/app.bin
   ```

   Put `release.suit`, `app.bin` and `f1.bin` in the same directory on the server. A relative URI resolves against the URL of the manifest.

## Transports

The transport struct has one getter slot per link: `https`, `ble`, `mqtt` and `custom`. Register the links that the device has. The scheme of each URI selects the slot:

| Scheme | Slot |
|---|---|
| `https`, `http` | `https` |
| `ble` | `ble` |
| `mqtt`, `mqtts` | `mqtt` |
| any other | `custom` |

A URI with no registered getter fails with `ESP_ERR_SUIT_OTA_NO_TRANSPORT` before a byte moves. One release can bring the app over HTTPS and a data payload over BLE.

A getter reads bytes of `t->url`, starting at `t->offset`. It returns the byte count, 0 at the end of the resource, or a negated `esp_err_t`. When `t->epoch` changes, the getter must close its old resource and open the new one.

The built-in HTTPS getter supports Range requests for resume and for chunked downloads (`max_request_size`). It sends the stored ETag of the manifest as `If-None-Match`, so a poll that finds no new release costs one small request. The getter always uses TLS and needs a way to verify the server: `cert_pem`, `crt_bundle_attach` or `use_global_ca_store`. `CONFIG_ESP_SUIT_OTA_HTTPS_ALLOW_HTTP` removes that rule, for a test setup only.

For a stream that another engine delivers, for example a UART or a Matter BDX transfer, pass `t = NULL` to `esp_suit_ota_begin()`. Then feed the envelope with its payloads inside it. A component whose URI is `#name` carries its payload inside the envelope.

## Slot pairs

Every pair declares its kind:

- `AB` keeps two slots. The slots record says which slot is live.
- `COPY` writes the new data to a staging partition and verifies it there. Activation copies it into the live partition and verifies the copy, so the live address never moves.
- `SINGLE` writes in place. This is not power safe, so a single pair cannot join a group.
- `APP` is the OTA app pair. Otadata selects its partitions.

The partition labels are optional. A pair that names neither partition takes its labels from its name:

```c
{ .name = "app", .kind = ESP_OTA_SLOTS_APP }      /* otadata selects the app partitions */
{ .name = "f1",  .kind = ESP_OTA_SLOTS_AB }       /* f1_0 and f1_1 */
{ .name = "cfg", .kind = ESP_OTA_SLOTS_COPY }     /* cfg and cfg_staging */
{ .name = "cal", .kind = ESP_OTA_SLOTS_SINGLE }   /* cal */
```

If your labels are different, name both partitions:

```c
{ .name = "f1",  .kind = ESP_OTA_SLOTS_AB,     .primary = "f1_live", .secondary = "f1_spare" }
{ .name = "cfg", .kind = ESP_OTA_SLOTS_COPY,   .primary = "cfg",     .secondary = "cfg_stage" }
{ .name = "cal", .kind = ESP_OTA_SLOTS_SINGLE, .primary = "phy_cal" }
```

Name both partitions or name neither. A pair that names only one is refused with `ESP_ERR_INVALID_ARG`. A derived label longer than 16 characters is also refused, so a long pair name must carry its labels.

Pairs that share a `group` flip together or not at all. A release that updates only part of a group is refused with `ESP_ERR_SUIT_OTA_GROUP`.

The slots record, the journal, the installed record and the list of rejected releases are in NVS. `nvs_flash_erase()` deletes them. After that, every pair starts at its first slot and the device accepts any sequence number at or above `CONFIG_ESP_SUIT_OTA_INITIAL_SEQ`.

## Mapping components to targets

If `cfg.map` is `NULL`, the last part of the component identifier selects a declared pair. For example, `[board, f1]` selects the pair `f1`, and `[board, app]` selects the app pair. An explicit map can change the verification rule, name a fixed partition, or add a custom sink:

```c
static const esp_suit_ota_span_t cid_dsp[] = { ESP_SUIT_OTA_SPAN("esp32p4"), ESP_SUIT_OTA_SPAN("dsp") };
esp_suit_ota_map_entry_t map[] = {
    { .id = { cid_app, 2 }, .verify = ESP_SUIT_VERIFY_APP_IMAGE, .slots = "app", .bulk_erase = true },
    { .id = { cid_dsp, 2 }, .verify = ESP_SUIT_VERIFY_DIGEST, .sink = &my_ddr_sink },
};
```

A sink is five functions (`open`, `write`, `read`, `close`, `abort`) for a target that is not a flash partition. The core computes the digest over the bytes that it writes, so a sink cannot misreport what it received. The device cannot read an installed image back from a sink, so a sink component is always fetched.

## Behavior

- Sequence number: a manifest below the installed number is refused with `ESP_ERR_SUIT_OTA_SEQUENCE`. An equal number is accepted. If every component is already installed, the run ends with `ESP_ERR_SUIT_OTA_UP_TO_DATE` and nothing is fetched.
- Signatures: the authentication wrapper can carry one COSE_Sign1 block per signer. One block that verifies with a trusted key is enough. ES256 (-7) and ESP256 (-9) are accepted. The wrapper must be well formed in full.
- Skip if installed: a component whose installed image matches the manifest digest is not fetched. The device hashes the live partition to decide.
- Power loss: the write position goes to an NVS journal every `CONFIG_ESP_SUIT_OTA_JOURNAL_KB`. The next run with the same manifest resumes with a Range request. A release that fails its self-test and rolls back is recorded as rejected and is not installed again.
- Activation order: COPY pairs are copied and verified, the slots record is written as pending, the installed record is saved, and then `esp_ota_set_boot_partition()` runs.
- Order of calls: `esp_suit_ota_begin()` refuses with `ESP_ERR_SUIT_OTA_STATE` while the running app waits for confirmation, or while an activated release waits for `esp_suit_ota_confirm()` or `esp_suit_ota_revert()`.

## Conformance

The component follows draft-ietf-suit-manifest-38 for the updater role. The test app checks each rule below.

Supported, from the conformance matrix in section 8.5:

- Conditions: vendor identifier (UUID or `cbor-pen`, tag 112), class identifier, device identifier, image match, component slot, abort.
- Directives: set component index (integer, array and `true`), override parameters, fetch, try-each, run-sequence.
- Parameters: vendor and class identifier, image digest and image size (both required for a fetch), URI, component slot, device identifier, fetch arguments, strict order, soft failure.

Not supported. Each of these makes the manifest fail and is never ignored, as section 6.1 of revision 38 requires:

- The directives `write`, `copy`, `swap` and `invoke`, and the condition `check-content`.
- Custom commands and custom parameters.
- `dependencies` in the common block, and the trust-domain members 15 and 18.
- Encrypted payloads (draft-ietf-suit-firmware-encryption).

An updater does not run the `load` and `invoke` procedures. The device parses them for shape only. The members `reference-uri`, `component-id`, `coswid` and `uninstall` are metadata for the updater, and the device skips them. Unknown envelope members are envelope extensions, and the device skips them. Unknown manifest members make the manifest fail.

Algorithms: SHA-256 for every digest, and ES256 or ESP256 for signatures. This is the signature part of the `suit-sha256-esp256-ecdh-a128ctr` profile of draft-ietf-suit-mti. The component has no payload encryption, so it does not claim a complete profile.

Encoding: every item must be deterministic CBOR (RFC 8949 section 4.2.1). Lengths are definite, integers and lengths take their shortest form, map keys are in canonical order and never repeat, and there are no floats. The envelope key of an integrated payload must equal the URI of its component byte for byte.

A post-fetch image match inside try-each is accepted only in the last branch. These Kconfig options set the limits:

| Option | Limit |
|---|---|
| `CONFIG_ESP_SUIT_OTA_ENVELOPE_BUF` | the arena for the authentication wrapper, the manifest and the severed members |
| `CONFIG_ESP_SUIT_OTA_AUTH_MAX` | the authentication wrapper |
| `CONFIG_ESP_SUIT_OTA_MAX_COMPONENTS` | the component count |
| `CONFIG_ESP_SUIT_OTA_MAX_NESTING` | the depth of try-each and run-sequence |
| `CONFIG_ESP_SUIT_OTA_URI_MAX` | the URI length |
| `CONFIG_ESP_SUIT_OTA_TEXT_CAP` | the kept update description |

## API

- `include/esp_suit_ota.h`: the campaign, the transport struct, the map and the events.
- `include/esp_suit_ota_helpers.h`: the built-in HTTPS getter.
- `include/esp_ota_slots.h`: the partition pairs and the slots record.
- `include/esp_suit_ota_err.h`: the error codes, and `esp_suit_ota_err_to_name()`.

The component posts events on the default event loop under `ESP_SUIT_OTA_EVENT`. The events are `MANIFEST_VERIFIED`, `COMPONENT_NEXT`, `COMPONENT_SKIPPED`, `COMPONENT_HEADER`, `PROGRESS`, `COMPONENT_STAGED`, `READY`, `ACTIVATED`, `ABORTED` and `CONNECTED`. `COMPONENT_HEADER` carries the chip ID, the chip revision and the app descriptor of a new app image.

The task that calls `esp_suit_ota_feed()` needs about 8 KB of stack.

## Example

[examples/suit_ota_advanced](examples/suit_ota_advanced) updates an app and two data pairs in one release over two transports, HTTPS and BLE. It shows a group, a COPY pair and the self-test.

## Tests

- `test_apps/` has the compliance tests. Each one names the draft rule that it checks. They run on an ESP32-C5 or an ESP32-P4 board:

  ```
  cd test_apps
  idf.py -B build_esp32c5 -DIDF_TARGET=esp32c5 build
  pytest --target esp32c5 --build-dir build_esp32c5 --port PORT
  ```

  `test_apps/main/vectors/gen_vectors.py` makes the test envelopes. Run it after you change a vector.

- `tools/tests/` has the tests of the `esp-suit` tool: `cd tools && python -m pytest`.

## License

Apache-2.0
