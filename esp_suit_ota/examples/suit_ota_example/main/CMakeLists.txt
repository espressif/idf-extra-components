# ble_payload.c compiles to nothing unless CONFIG_EXAMPLE_ENABLE_BLE is set, and example_targets.c
# picks its half from CONFIG_EXAMPLE_USE_OTA_SLOTS. The A/B slots code lives inside esp_suit_ota,
# in its slots/ directory, so nothing extra is listed here and the linker drops what the build
# does not call.
#
# keys/device_key.bin is the 32-byte P-256 private scalar that decrypts an encrypted payload.
# It is embedded only when CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD is set, so a build without
# encryption carries no key at all. A product puts this key in eFuse or in the Key Manager and
# sets cfg.decrypt_agree instead of embedding it; the README explains how.
#
set(embed "keys/anchor_p256.bin")
if(CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD)
    list(APPEND embed "keys/device_key.bin")
endif()

idf_component_register(SRCS "suit_ota_example.c" "ble_payload.c" "example_targets.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES ${embed}
                       PRIV_REQUIRES nvs_flash app_update esp_partition esp_netif esp_event
                                     esp_http_client mbedtls bt)
