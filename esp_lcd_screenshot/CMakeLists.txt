idf_component_register(SRCS "src/esp_lcd_screenshot.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd
                       PRIV_REQUIRES mbedtls)
