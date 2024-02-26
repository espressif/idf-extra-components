set(src "test_app_main.c" "test_spi_nand_flash.c")

idf_component_register(SRCS ${src}
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES test_utils vfs fatfs spiffs unity lwip wear_levelling cmock driver
                       REQUIRES dhara
                       WHOLE_ARCHIVE
                       )
