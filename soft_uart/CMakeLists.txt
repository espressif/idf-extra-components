idf_component_register(SRCS "src/rmt_uart.c" "src/rmt_uart_encoder.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_rmt)
