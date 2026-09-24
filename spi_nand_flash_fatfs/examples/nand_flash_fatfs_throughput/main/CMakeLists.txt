set(priv_requires spi_nand_flash_fatfs esp_timer)

# For IDF < 6.0, use IDF's built-in json component; for IDF >= 6.0 use managed cJSON component
if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    list(APPEND priv_requires json)
endif()

idf_component_register(SRCS "spi_nand_flash_fatfs_throughput_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_requires}
                       )
