set(src "test_app_main.c" "test_spi_nand_flash.c")

set(priv_reqs unity esp_timer)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND priv_reqs esp_driver_spi)
else()
    list(APPEND priv_reqs driver)
endif()

idf_component_register(SRCS ${src}
                       PRIV_REQUIRES ${priv_reqs}
                       WHOLE_ARCHIVE
                       )
