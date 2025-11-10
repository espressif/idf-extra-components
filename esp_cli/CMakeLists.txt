idf_build_get_property(target IDF_TARGET)

set(srcs "src/esp_cli.c")

idf_component_register(
                    SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES esp_linenoise esp_cli_commands
                    WHOLE_ARCHIVE)
