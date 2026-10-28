idf_component_register(SRCS "raylib_example_main.c" "fireworks.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_lcd esp_timer)
