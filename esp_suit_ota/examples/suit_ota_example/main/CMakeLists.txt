# ble_payload.c compiles to nothing unless CONFIG_EXAMPLE_ENABLE_BLE is set, and example_targets.c
# picks its half from CONFIG_EXAMPLE_USE_OTA_SLOTS. The requirement list cannot depend on a
# CONFIG_ value, because it is read before the configuration is available, so both slots
# components stay listed here and the linker drops what the build does not call.
idf_component_register(SRCS "suit_ota_example.c" "ble_payload.c" "example_targets.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES "keys/anchor_p256.bin"
                       PRIV_REQUIRES nvs_flash app_update esp_partition esp_netif esp_event
                                     esp_http_client mbedtls bt esp_ota_slots esp_suit_ota_slots)
