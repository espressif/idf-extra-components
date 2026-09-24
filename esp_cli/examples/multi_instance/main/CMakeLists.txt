idf_build_get_property(target IDF_TARGET)
set(priv_requires esp_cli esp_cli_commands esp_linenoise)

if(NOT "${target}" STREQUAL "linux")
    list(APPEND priv_requires esp_driver_uart esp_driver_usb_serial_jtag vfs)
endif()

idf_component_register(
    SRCS "multi_instance_main.c" "../../utils/cli_example_io.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
