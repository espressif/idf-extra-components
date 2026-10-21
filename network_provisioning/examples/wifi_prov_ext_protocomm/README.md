| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# Wi-Fi Provisioning in Session-Only Mode Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

`wifi_prov_ext_protocomm` demonstrates `NETWORK_PROV_MODE_SESSION_ONLY` of the `network_provisioning` manager. In this mode the manager starts the Bluetooth LE transport with only the `prov-session` (secure session) and `proto-ver` (version and capabilities) endpoints, and keeps the transport and the secure protocomm session running after provisioning completes, as well as on boots where the device is already provisioned. The application can therefore serve its own endpoints over the same secure session for as long as it likes, instead of only during provisioning.

The example registers one application endpoint, `custom-data`, and behaves as follows:

* **First boot (device not provisioned):** `network_prov_mgr_enable_provisioning()` is called before `network_prov_mgr_start_provisioning()`. This adds the provisioning endpoints (`prov-config`, `prov-scan`, `prov-ctrl`) to the Bluetooth LE service and the `wifi_prov` and `wifi_scan` capabilities to `proto-ver`, so the device can be provisioned with `esp_prov.py` or the Espressif provisioning apps. Once provisioning completes, the manager removes the provisioning endpoints again but keeps Bluetooth LE, the secure session and `custom-data` up.
* **Later boots (device provisioned):** `network_prov_mgr_enable_provisioning()` is not called. Bluetooth LE starts with only `prov-session`, `proto-ver` and `custom-data`. The manager does not touch Wi-Fi in this case, so the example connects to the stored network itself.
* **`network_prov_mgr_deinit()` is never called**, because it is the only call that stops the transport.

Only the Bluetooth LE scheme (`network_prov_scheme_ble`) supports this mode.

## How to use example

### Hardware Required

Any development board with a target that has both Wi-Fi and Bluetooth LE.

### Script Required

The `esp_prov.py` [script](../../tool/esp_prov/esp_prov.py) is used to provision the device and to exercise the `custom-data` endpoint. See the `wifi_prov` example [README](../wifi_prov/README.md#script-required) for its dependencies.

### Configure the project

```
idf.py menuconfig
```

Under "Example Configuration":

* **Protocomm security version**: security version 2 (SRP6a authentication and key exchange, AES-GCM encryption; the default) or security version 1 (X25519 key exchange with proof of possession, AES-CTR encryption).
* **Security version 2 mode**: development mode (the default) uses the built-in username `wifiprov` and password `abcd1234`. Production mode expects the salt and verifier to come from the device's manufacturing data, and the username and password to reach the user by other means, such as a QR code sticker. The example only prints an error in production mode until this is implemented.
* **Reset provisioned status on each boot**: erases the stored Wi-Fi credentials at startup so that provisioning runs on every boot. Useful for testing.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

On the first boot the device prints a QR code and the URL carrying the same payload, and advertises as `PROV_XXXXXX`, where `XXXXXX` is derived from the MAC address:

```
I (xxx) session_only_prov: Starting provisioning
I (xxx) network_prov_mgr: Provisioning started with service name : PROV_XXXXXX
I (xxx) session_only_prov: Provisioning started
I (xxx) session_only_prov: Scan this QR code from the provisioning application for Provisioning.
...
I (xxx) session_only_prov: Connected to Wi-Fi, custom-data endpoint stays reachable over BLE
```

On later boots there is no provisioning, only the transport comes up:

```
I (xxx) session_only_prov: Already provisioned, starting BLE for the custom-data endpoint only
I (xxx) network_prov_mgr: Provisioning started with service name : PROV_XXXXXX
I (xxx) session_only_prov: Provisioning started
I (xxx) session_only_prov: Connected to Wi-Fi, custom-data endpoint stays reachable over BLE
```

`Provisioning started` is logged on `NETWORK_PROV_START`, which the manager also emits when it only starts the transport. The example registers `custom-data` on that event.

### Provisioning with esp_prov.py

Replace `XXXXXX` with the value from the device log.

Security version 2 (default configuration):

```
python ../../tool/esp_prov/esp_prov.py --transport ble --service_name PROV_XXXXXX --sec_ver 2 --sec2_username wifiprov --sec2_pwd abcd1234 --ssid <SSID> --passphrase <PASSWORD>
```

Security version 1:

```
python ../../tool/esp_prov/esp_prov.py --transport ble --service_name PROV_XXXXXX --sec_ver 1 --pop abcd1234 --ssid <SSID> --passphrase <PASSWORD>
```

Add `--custom_data <MyCustomData>` to either command to exercise the `custom-data` endpoint during provisioning. The device logs the received data and answers `SUCCESS`.

### Sending Custom Data to a provisioned device

On a provisioned device the provisioning endpoints are absent, but `custom-data` is served over the same secure session:

```
python ../../tool/esp_prov/esp_prov.py --transport ble --service_name PROV_XXXXXX --sec_ver 2 --sec2_username wifiprov --sec2_pwd abcd1234 --custom_data "hello"
```

Expected output:

```
==== Sending Custom data to Target ====
CustomData response: SUCCESS
==== Custom data sent successfully ====
```

`esp_prov.py` then goes on with the Wi-Fi provisioning steps. Since the device does not advertise the `wifi_scan` capability, the script stops with `Wi-Fi Scan List is not supported by provisioning service`. The custom data exchange has completed by then, so this message can be ignored. For the same reason the script prints `Use wifi_provisioning for device without "wifi_prov" capabilities` right after the session is established, which can be ignored as well.
