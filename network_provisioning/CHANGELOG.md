# 07-October-2025

- Use managed cJSON component for IDF v6.0 and above

# 01-April-2025

- Extend provisioning check for `ESP_WIFI_REMOTE_ENABLED` as well along with existing `ESP_WIFI_ENABLED`
- This enables provisioning for the devices not having native Wi-Fi (e.g., ESP32-P4) and using external/remote Wi-Fi solution such as esp-hosted for Wi-Fi connectivity.

# 17-March-2025

- Update the network provisioning component to work with the protocomm component which fixes incorrect AES-GCM IV usage in security2 scheme.

# 19-June-2024

- Change the proto files to make the network provisioning component stay backward compatible with the wifi_provisioing

# 23-April-2024

- Add `wifi_prov` or `thread_prov` in provision capabilities in the network provisioning manager for the provisioner to distinguish Thread or Wi-Fi devices

# 16-April-2024

- Move wifi_provisioning component from ESP-IDF at commit 5a40bb8746 and rename it to network_provisioning with the addition of Thread provisioning support.
- Update esp_prov tool to support both Wi-Fi provisioning and Thread provisioning.
- Create thread_prov and wifi_prov examples
