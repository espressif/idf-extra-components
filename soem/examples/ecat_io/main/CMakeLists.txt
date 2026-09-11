idf_component_register(
    SRCS
        "ecat_io_example_main.c"

    PRIV_REQUIRES
        soem
        esp_eth
        esp_event
        esp_timer
)
