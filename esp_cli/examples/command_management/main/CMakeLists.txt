set(priv_requires esp_cli esp_cli_commands esp_linenoise esp_stdio)

idf_component_register(
    SRCS "command_management_main.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
