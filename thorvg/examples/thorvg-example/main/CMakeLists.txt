idf_component_register(
    SRCS "display_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_timer esp_driver_ppa)

idf_component_get_property(board_lib espressif__esp32_p4_function_ev_board_noglib COMPONENT_LIB)
target_compile_options(${board_lib} PRIVATE "-Wno-unused-variable")

spiffs_create_partition_image(storage ../spiffs_content FLASH_IN_PROJECT)
