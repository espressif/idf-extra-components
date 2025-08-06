idf_component_register(
    SRCS "isotp_echo_main.c"
    INCLUDE_DIRS "."
    REQUIRES isotp esp_timer esp_driver_twai
)
