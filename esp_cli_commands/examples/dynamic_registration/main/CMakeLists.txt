# Main CMakeLists.txt for debug_and_unregister_dynamic example
cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS "dynamic_registration_main.c"
    INCLUDE_DIRS "" "../../utils"
    REQUIRES esp_cli_commands
)
