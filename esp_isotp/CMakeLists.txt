# Apply patch to esp_isotp-c submodule
find_package(Git REQUIRED)

idf_component_register(
    SRCS "esp_isotp.c" "isotp-c/isotp.c"
    INCLUDE_DIRS "." "isotp-c"
    REQUIRES esp_timer esp_driver_twai
)
