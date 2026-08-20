idf_component_register(
    SRCS "thorvg_example_main.c" "example_lcd.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_timer pthread)

littlefs_create_partition_image(storage ../lottie_files FLASH_IN_PROJECT)
