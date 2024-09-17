idf_component_register(
    SRCS "display_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_timer esp_driver_ppa)

spiffs_create_partition_image(storage ../spiffs_content FLASH_IN_PROJECT)
