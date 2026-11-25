set(srcs
    "src/suit_cbor.c"
    "src/suit_crypto.c"
    "src/suit_cose.c"
    "src/suit_frame.c"
    "src/suit_manifest.c"
    "src/suit_exec.c"
    "src/suit_stream.c"
    "src/suit_sink_device.c"
    "src/suit_sink_partition.c"
    "src/suit_map.c"
    "src/suit_persist.c"
    "src/suit_decrypt.c"
    "src/suit_keywrap.c"
    "src/esp_suit_ota.c"
    "src/esp_suit_ota_err.c"
    "helpers/esp_suit_ota_activate.c"
    "helpers/esp_suit_ota_https.c")

# slots/ is the A/B slots backend, kept apart from the SUIT core. esp_ota_slots.c owns the
# partition pairs and the power-safe record in NVS, and knows nothing about SUIT.
# esp_suit_ota_slots.c is the adapter between the two. Neither is needed to run an update: an
# application that leaves cfg.slots_ops at NULL maps every component to a fixed partition
# instead, and the linker then leaves both files out of the image.
set(slots_srcs
    "slots/esp_ota_slots.c"
    "slots/esp_suit_ota_slots.c")

idf_component_register(SRCS ${srcs} ${slots_srcs}
                       INCLUDE_DIRS "include" "slots/include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES esp_partition esp_event esp_app_format esp_http_client
                       PRIV_REQUIRES nvs_flash mbedtls log esp_timer app_update bootloader_support
                                     esp_image_verify efuse esp_bootloader_format)

if(COMMAND idf_define_esp_err_codes)
    idf_define_esp_err_codes(HEADERS include/esp_suit_ota_err.h slots/include/esp_ota_slots.h)
endif()
