# ble_payload.c compiles to nothing unless CONFIG_EXAMPLE_ENABLE_BLE is set. The requirement list
# cannot depend on a CONFIG_ value, because it is read before the configuration is available.
idf_component_register(SRCS "suit_ota_advanced.c" "ble_payload.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES "keys/anchor_p256.bin"
                       PRIV_REQUIRES nvs_flash app_update esp_partition esp_netif esp_event
                                     esp_http_client mbedtls bt)
