idf_component_register(
    SRCS "command_with_arg_main.c"
    INCLUDE_DIRS "." "../../utils"
    REQUIRES esp_cli_commands
)
