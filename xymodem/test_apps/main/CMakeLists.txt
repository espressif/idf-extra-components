idf_component_register(SRCS "test_app_main.c"
                            "test_xymodem_common.c"
                            "test_xmodem.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES unity esp_driver_uart esp_driver_gpio
                    WHOLE_ARCHIVE)
