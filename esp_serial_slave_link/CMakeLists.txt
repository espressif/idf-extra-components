set(public_requires "sdmmc")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND public_requires "esp_driver_sdspi" "esp_driver_sdmmc")
else()
    list(APPEND public_requires "driver")
endif()

idf_component_register(SRCS "essl.c"
                "essl_sdio.c"
                "essl_spi.c"
                "essl_sdio_defs.c"
            INCLUDE_DIRS "include"
            REQUIRES ${public_requires}
            PRIV_INCLUDE_DIRS "."
                "include/esp_serial_slave_link"
)
