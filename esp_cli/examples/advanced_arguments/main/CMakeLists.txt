set(priv_requires esp_cli esp_cli_commands esp_linenoise argtable3 esp_stdio)

idf_component_register(
    SRCS "advanced_arguments_main.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
