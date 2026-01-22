idf_component_register(
    SRCS "history_usage.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES esp_linenoise fatfs esp_driver_uart
)
