idf_build_get_property(target IDF_TARGET)
if("${target}" STREQUAL "linux")
    return()
endif()

set(priv_requires spiffs esp_cli esp_cli_commands esp_linenoise esp_stdio)

idf_component_register(
    SRCS "lifecycle_hooks_main.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
