# Main CMakeLists.txt for command_set example
cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS "command_set_main.c"
    INCLUDE_DIRS "" "../../utils"
    REQUIRES esp_cli_commands
)
