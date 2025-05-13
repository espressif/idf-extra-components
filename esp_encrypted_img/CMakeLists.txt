set(ESP_ENCRYPT_SRCS "src/esp_encrypted_img.c")

if(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    list(APPEND ESP_ENCRYPT_SRCS "src/esp_encrypted_img_utilities.c")
endif()

idf_component_register(SRCS "${ESP_ENCRYPT_SRCS}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "private_include"
                    PRIV_REQUIRES mbedtls)

if(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    idf_component_optional_requires(PRIVATE efuse)
endif()
