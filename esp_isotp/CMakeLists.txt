idf_component_register(
    SRCS "src/esp_isotp.c" "isotp-c/isotp.c"
    INCLUDE_DIRS "inc"
    PRIV_INCLUDE_DIRS "isotp-c"
    REQUIRES esp_driver_twai
    PRIV_REQUIRES esp_timer
)
