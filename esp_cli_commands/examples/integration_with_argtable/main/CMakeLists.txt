idf_component_register(
    SRCS "integration_with_argtable_main.c"
    INCLUDE_DIRS "." "../../utils"
    REQUIRES esp_cli_commands argtable3
)
