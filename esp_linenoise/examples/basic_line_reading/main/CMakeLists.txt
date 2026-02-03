cmake_minimum_required(VERSION 3.22)

idf_build_get_property(target IDF_TARGET)
set(priv_requires esp_linenoise)

if(NOT "${target}" STREQUAL "linux")
    list(APPEND priv_requires esp_driver_uart)
endif()

idf_component_register(
    SRCS "basic_line_reading.c" "../../utils/common_io.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
