idf_component_register(SRCS "xmodem_example_main.c" "example_transport_uart.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_uart esp_timer)
