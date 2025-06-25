set(srcs)

if(CONFIG_SOC_RMT_SUPPORTED)
    list(APPEND srcs "src/rmt_uart.c" "src/rmt_uart_encoder.c")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    REQUIRES "esp_driver_rmt")
