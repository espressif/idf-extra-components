set(includes "include")

idf_component_register(SRCS "esp_flash_dispatcher.c"
                       INCLUDE_DIRS ${includes}
                       PRIV_REQUIRES "spi_flash" "freertos" "esp_system"
                       )

set(WRAP_FUNCTIONS
    esp_flash_write
    esp_flash_erase_region
    esp_flash_read
    esp_flash_erase_chip
    esp_flash_write_encrypted)

foreach(wrap ${WRAP_FUNCTIONS})
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${wrap}")
endforeach()
