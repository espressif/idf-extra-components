
idf_component_register(SRCS "test_esp_cli.c" "test_main.c"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES unity vfs esp_driver_uart esp_driver_usb_serial_jtag
                    WHOLE_ARCHIVE)
