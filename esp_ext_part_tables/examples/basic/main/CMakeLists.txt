idf_build_get_property(target IDF_TARGET)

set(requires heap)
if(NOT ${target} STREQUAL "linux")
    list(APPEND requires driver sdmmc esp_driver_sdspi)
endif()

idf_component_register(
    SRCS "main.c" "example_utils.c"
    REQUIRES ${requires}
)
