idf_component_register(SRCS "spi_nand_flash_debug_app_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES spi_nand_flash esp_timer
                       )
