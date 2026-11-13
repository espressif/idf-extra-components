| Supported Targets | ESP32-C5 | ESP32-P4 |
| ----------------- | -------- | -------- |

# SUIT OTA: advanced example

One signed release updates an app and two data partitions together, over two transports. The application runs the campaign loop itself, so every step is visible in `main/suit_ota_advanced.c`.

The example shows:

- Three pairs: `app` (the OTA app pair), `f1` (an AB pair over `f1_live` and `f1_spare`) and `cfg` (a COPY pair over `cfg` and `cfg_stage`).
- A group: `app` and `f1` are in the group `cal`, so they flip together or not at all.
- Two transports in one release: the HTTPS getter of the component, and a BLE getter in `main/ble_payload.c`.
- The self-test: the example makes sure that the new data is correct, then confirms or reverts the release.

The ESP32-C5 connects over Wi-Fi and offers BLE. The ESP32-P4 connects over Ethernet and has no BLE.

## Configure

Run `idf.py menuconfig`:

- Example Connection Configuration: the Wi-Fi network, or the Ethernet PHY.
- Example Configuration:
  - `Manifest URL`: the URL of the release. The default, `FROM_STDIN`, asks for the URL on the console.
  - `Use the certificate bundle`, `Skip the server certificate common name check`: how the device verifies the HTTPS server.
  - `Range chunk size`: 0 downloads each payload in one request. Another value downloads it in Range requests of that size.
  - `Offer a second transport`: the BLE getter, for components with a `ble://` URI.

The trust anchor is `main/keys/anchor_p256.bin`. Its signing key, `main/keys/manifest_key.pem`, is in the example so that you can try it. Make your own key for a product, as the component README describes.

## Build and flash

```
idf.py set-target esp32c5
idf.py -p PORT flash monitor
```

## Make a release

A release needs a second build of the app. Give it another version string, and build it into `build_v2`:

```
idf.py -B build_v2 -DSDKCONFIG=build_v2/sdkconfig menuconfig    # Application manager: set the version
idf.py -B build_v2 -DSDKCONFIG=build_v2/sdkconfig build
```

Then make the release and put it, with its payloads, in one directory on a server:

```
cd release
esp-suit create release.yml
mkdir -p www
cp release.suit www/
cp ../build_v2/suit_ota_advanced.bin www/app.bin
cp f1_v2.bin www/f1.bin
cp cfg_v1.bin www/cfg.bin
```

`release/release.yml` lists the app, `f1` and `cfg`. The URIs are relative, so the device fetches each payload from the directory of `release.suit`. Serve `www` over HTTPS and type the URL of `release.suit` on the console. For a quick test on your own network, you can serve it over HTTP instead: set `CONFIG_ESP_SUIT_OTA_HTTPS_ALLOW_HTTP` and run `python -m http.server` in `www`.

`release/partial_release.yml` updates `f1` without `app`. The device refuses it with `ESP_ERR_SUIT_OTA_GROUP`, because `f1` and `app` are in one group.

## Fetch a component over BLE

Change the URI of a component in `release.yml` to `ble://f1.bin`, and make the release again. The device advertises as `suit-ota` (`CONFIG_EXAMPLE_BLE_DEVICE_NAME`). When the campaign reaches that component, the device notifies a connected peer on the request characteristic with one line of text:

```
GET <uri> <offset> <length>
```

The peer then writes those bytes of the resource to the data characteristic. The offset is not always zero, because a campaign cut by a reset resumes in the middle of a component.

| | UUID |
|---|---|
| service | `3a2b1c0d-5e4f-6071-8293-a4b5c6d70001` |
| request (notify) | `3a2b1c0d-5e4f-6071-8293-a4b5c6d70002` |
| data (write) | `3a2b1c0d-5e4f-6071-8293-a4b5c6d70003` |

## Expected output

```
I (5012) suit_advanced: event: manifest 2001, 3 components, 3 to fetch
I (5022) suit_advanced: event: fetching esp32c5/app (1601840 bytes) from https://.../app.bin
I (31002) esp_suit_ota: esp32c5/app staged and verified (1601840 bytes)
I (31702) esp_suit_ota: esp32c5/f1 staged and verified (6300 bytes)
I (32352) esp_suit_ota: esp32c5/cfg staged and verified (2055 bytes)
I (32362) esp_suit_ota: validate sequence passed
I (32402) esp_suit_ota: esp32c5/cfg: copying cfg_stage to cfg
I (32652) esp_suit_ota: release 2001 activated, restart to run the new app
I (32662) suit_advanced: RESULT: app release activated, restarting
```

After the restart, the new app checks `f1` and `cfg` and confirms the release.

## Test

`pytest_suit_ota_advanced.py` makes two releases from the app that it flashes, serves them from the test machine, and types each URL on the console. It needs a board on a network, so CI skips it. Run it by hand with the CI configuration:

```
idf.py -B build -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci" build
SUIT_OTA_TEST=1 SUIT_OTA_TEST_WIFI="<ssid> <password>" pytest --target esp32c5 --build-dir build
```

On an ESP32-P4, leave out `SUIT_OTA_TEST_WIFI`. The board uses Ethernet.
