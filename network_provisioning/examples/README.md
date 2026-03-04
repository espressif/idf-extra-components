# Provisioning Application Examples

This consists of the examples `wifi_prov`, `thread_prov` and `wifi_prov_ext_protocomm`.

* wifi_prov
    Abstracts out most of the complexity of Wi-Fi provisioning and allows easy switching between the SoftAP (using HTTP) and BLE transports. It also demonstrates how applications can register and use additional custom data endpoints.

* thread_prov
    Abstracts out most of the complexity of Thread provisioning over BLE transport. It also demonstrates how applications can register and use additional custom data endpoints.

* wifi_prov_ext_protocomm
    Demonstrates session-only mode, which keeps the protocomm transport alive after provisioning so that the application can reuse the secure session.

Provisioning applications are available for `Linux / Windows / macOS` platform as `esp_prov.py` [script](../tool/esp_prov/esp_prov.py)
