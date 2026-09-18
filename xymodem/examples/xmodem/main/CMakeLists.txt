set(srcs "xmodem_example_main.c")

if(CONFIG_EXAMPLE_XYMODEM_BACKEND_UART)
    list(APPEND srcs "example_transport_uart.c")
endif()

if(CONFIG_EXAMPLE_XYMODEM_BACKEND_UHCI)
    list(APPEND srcs "example_transport_uhci.c")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_uart esp_timer esp_ringbuf)
