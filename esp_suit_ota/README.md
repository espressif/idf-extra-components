# esp_suit_ota

[![Component Registry](https://components.espressif.com/components/espressif/esp_suit_ota/badge.svg)](https://components.espressif.com/components/espressif/esp_suit_ota)

SUIT manifest based OTA for ESP-IDF. One signed manifest describes a whole release: the app, data partitions, or both. The device verifies the manifest before it downloads a payload byte. It writes every payload to a passive slot, verifies every payload, and then switches the whole set at once. A release applies completely or not at all.

The component implements [draft-ietf-suit-manifest-38](https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/) for the updater role. The draft is in the RFC Editor queue. Payloads go through the native `app_update` API (`esp_ota_begin`, `esp_ota_write`, `esp_ota_end`). App images therefore get the same checks as with `esp_https_ota`: Secure Boot signature, anti-rollback, chip ID and chip revision.

Supported: ESP-IDF 6.0 and later. Tested on ESP32-C5 and ESP32-P4.

The component owns the manifest, the verification, the downloads and the digests. It does not own
the partition layout. Where each component is written, and how a bad release goes back, come from a
slots backend that you supply. The ready-made one is in [slots/](slots): `esp_ota_slots.c` owns
the partition pairs and the record in NVS, and `esp_suit_ota_slots.c` joins it to the update
engine. Neither is needed. With `cfg.slots_ops` left at `NULL` the linker leaves both out.

## How a release works

```
release.json       build/app.bin     f1.bin
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

The `esp-suit` tool needs Python 3.9 or later, with `cbor2` and `cryptography` 44 or later. You can also run it without installing: `python tools/esp_suit.py <command>`. See [tools/README.md](tools/README.md).

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
   #include "esp_ota_slots.h"
   #include "esp_suit_ota_slots.h"

   static const esp_ota_slots_decl_t slots[] = {
       { .name = "app", .kind = ESP_OTA_SLOTS_APP },
       { .name = "f1",  .kind = ESP_OTA_SLOTS_AB },     /* f1_0 and f1_1 */
   };
   nvs_flash_init();
   esp_ota_slots_init(slots, 2);                 /* also settles a release cut by power loss */
   cfg.slots_ops = esp_suit_ota_slots_ops();     /* the backend esp_suit_ota asks about targets */
   if (self_test_ok()) {
       esp_ota_mark_app_valid_cancel_rollback(); /* keep the new app */
       esp_ota_slots_confirm();                  /* keep the new data */
   } else {
       esp_ota_mark_app_invalid_rollback_and_reboot();
   }
   const esp_partition_t *f1 = esp_ota_slots_get_active("f1");
   ```

   Nothing is added to `idf_component.yml` for this. Both files are part of this component, in
   [slots/](slots). Without a backend, `cfg.slots_ops` stays `NULL`, every component needs a map
   entry that names a fixed partition or a sink, and there are no A/B slots and no rollback for
   data.

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
       esp_suit_ota_end(h);                      /* close the getters and free the handle */
       return err;
   }
   ```

   `ESP_ERR_SUIT_OTA_UP_TO_DATE` and `ESP_ERR_SUIT_OTA_NOT_MODIFIED` mean that there is nothing to do. If `restart` is true, restart the device. The self-test in step 3 then decides about the new app. If `restart` is false, the release changed only data and the new data is live now. Make sure that the data is correct, then call `esp_suit_ota_confirm()` or `esp_suit_ota_revert()`.

5. Make the release on the build machine.

   ```json
   {
     "sequence_number": "auto-utc",
     "vendor": "example.com",
     "class": "my-product-hw2",
     "text": { "en": { "update-description": "1.2.3: fixes" } },
     "components": [
       { "id": ["board", "app"], "file": "build/app.bin", "uri": "app.bin", "skip_if_installed": true },
       { "id": ["board", "f1"],  "file": "f1.bin",        "uri": "f1.bin" }
     ],
     "signing": { "keys": ["keys/manifest_key.pem"] }
   }
   ```

   Every key is listed in [tools/docs/release_json.md](tools/docs/release_json.md).

   ```
   esp-suit create release.json --build-dir build
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

Pairs come from `slots/esp_ota_slots.c`. [slots/README.md](slots/README.md) has the full reference. The short form:

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

The installed record is in NVS under the namespace `esp_suit`, and the slots record is under `ota_slots`. `nvs_flash_erase()` deletes both. After that, every pair starts at its first slot and the device accepts any sequence number at or above `CONFIG_ESP_SUIT_OTA_INITIAL_SEQ`.

Only an `AB` pair is power safe from end to end. A `SINGLE` pair is written in place and cannot go back. A `COPY` pair is written to its staging partition first, but the copy into the live partition happens during activation, so a power cut in that window leaves the live partition part written.

## Mapping components to targets

If `cfg.map` is `NULL`, the last part of the component identifier is handed to the slots backend as a target name. For example, `[board, f1]` selects the pair `f1`, and `[board, app]` selects the app pair. An explicit map can change the verification rule, name fixed partitions, or add a custom sink:

```c
static const esp_suit_ota_span_t cid_dsp[] = { ESP_SUIT_OTA_SPAN("esp32p4"), ESP_SUIT_OTA_SPAN("dsp") };
esp_suit_ota_map_entry_t map[] = {
    { .id = { cid_app, 2 }, .verify = ESP_SUIT_VERIFY_APP_IMAGE, .slots = "app", .bulk_erase = true },
    { .id = { cid_dsp, 2 }, .verify = ESP_SUIT_VERIFY_DIGEST, .sink = &my_ddr_sink },
};
```

Each entry names a target in one of three ways: `slots`, which hands the name to the backend, `sink`, or `target_partition`. A `target_partition` entry has three forms. For an app, it is the spare OTA slot, and `otadata` switches the app. For data on its own, it is the live partition, written in place. With `live_partition` beside it, it is a staging partition, and `live_partition` is where the installed image is now, which is what makes `skip_if_installed` work. Add `copy_to_live` and `esp_suit_ota_activate()` copies the staged bytes into the live partition and hashes the copy back, which is a COPY pair without any backend:

```c
{ .id = { cid_cfg, 2 }, .verify = ESP_SUIT_VERIFY_DIGEST,
  .target_partition = cfg_stage, .live_partition = cfg, .copy_to_live = true },
```

An A/B pair needs a backend, because it needs a record of which slot is live that survives a reboot.

A sink is five functions (`open`, `write`, `read`, `close`, `abort`) for a target that is not a flash partition. The core computes the digest over the bytes that it writes, so a sink cannot misreport what it received. The device cannot read an installed image back from a sink, so a sink component is always fetched.

## Behavior

- Sequence number: a manifest below the installed number is refused with `ESP_ERR_SUIT_OTA_SEQUENCE`. An equal number is accepted. If every component is already installed, the run ends with `ESP_ERR_SUIT_OTA_UP_TO_DATE` and nothing is fetched.
- Signatures: the authentication wrapper can carry one COSE_Sign1 block per signer. One block that verifies with a trusted key is enough. ES256 (-7) and ESP256 (-9) are accepted. The wrapper must be well formed in full.
- Skip if installed: a component whose installed image matches the manifest digest is not fetched. The device hashes the live partition to decide.
- Power loss: a campaign keeps no state across a reset, and the next run starts at the first byte again. A campaign that is cut short writes only staging partitions, so the device keeps running what it ran before.
- Activation order: COPY pairs are copied and verified, the slots record is written as activating, the installed record is saved, `esp_ota_set_boot_partition()` runs, and the record moves to pending. If power is lost inside that window, the next `esp_ota_slots_init()` puts every pair back. If the bootloader rolls the new app back, `esp_ota_slots_init()` puts the data that came with it back too.
- One bad pair: `esp_suit_ota_revert_pair("f1")` sends `f1` and the rest of its group back to the previous slot, and leaves every other pair live.
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

An updater does not run the `load` and `invoke` procedures. The device parses them for shape only. The members `reference-uri`, `component-id`, `coswid` and `uninstall` are metadata for the updater, and the device skips them. Unknown envelope members are envelope extensions, and the device skips them. Unknown manifest members make the manifest fail.

Algorithms: SHA-256 for every digest, and ES256 or ESP256 for signatures. With `CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD`, payloads may also be encrypted, with ECDH-ES and AES key wrap for the content key and AES-CTR or AES-GCM for the payload. That completes two profiles of draft-ietf-suit-mti: `suit-sha256-esp256-ecdh-a128ctr` and `suit-sha256-esp256-ecdh-a128gcm`. The EdDSA, HMAC and HSS-LMS profiles are not implemented. A recipient may choose which profile it implements, so this is a conformant choice.

Encrypted payloads: the manifest carries `suit-parameter-encryption-info` (19), which holds `#6.96(COSE_Encrypt)` with a detached ciphertext. A fresh content key is wrapped for the device with ECDH-ES, and the device unwraps it with its own P-256 private key before a byte is fetched. The image digest covers the plaintext, and the image size covers the fetched resource, so for AES-GCM it includes the 16-byte tag. Set `esp_suit_ota_config_t.decrypt_key` to the raw scalar, or `decrypt_agree` to keep the key in eFuse or in the Key Manager. `content_algs` and `kex_algs` pin the algorithms the device accepts, which section 11 of the encryption draft requires.

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

- `include/esp_suit_ota.h`: the campaign, the transport struct, the map, the slots backend and the events.
- `include/esp_suit_ota_helpers.h`: the built-in HTTPS getter.
- `include/esp_suit_ota_err.h`: the error codes, and `esp_suit_ota_err_to_name()`.

The component posts events on the default event loop under `ESP_SUIT_OTA_EVENT`. The events are `MANIFEST_VERIFIED`, `COMPONENT_NEXT`, `COMPONENT_SKIPPED`, `COMPONENT_HEADER`, `PROGRESS`, `COMPONENT_STAGED`, `READY`, `ACTIVATED`, `ABORTED` and `CONNECTED`. `COMPONENT_HEADER` carries the chip ID, the chip revision and the app descriptor of a new app image.

The task that calls `esp_suit_ota_feed()` needs about 8 KB of stack.

## Example

[examples/suit_ota_example](examples/suit_ota_example) updates an app and two data pairs in one release over two transports, HTTPS and BLE. It shows a group, a COPY pair and the self-test.

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
