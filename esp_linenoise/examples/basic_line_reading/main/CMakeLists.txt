# Basic Line Reading Example for esp_linenoise
cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS "basic_line_reading.c"
    INCLUDE_DIRS "" "../../utils"
    REQUIRES esp_linenoise esp_driver_uart vfs
)
