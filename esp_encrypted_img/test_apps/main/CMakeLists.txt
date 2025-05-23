if (CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    set(EMBED_FILES "image_ecc.bin")
else()
    set(EMBED_FILES "image.bin")
endif()

idf_component_register(SRCS "esp_encrypted_img_test.c" "test.c" "test_mocks.c"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES unity esp_encrypted_img efuse
                    EMBED_TXTFILES certs/test_rsa_private_key.pem
                    EMBED_FILES "${EMBED_FILES}"
                    WHOLE_ARCHIVE
                    )
