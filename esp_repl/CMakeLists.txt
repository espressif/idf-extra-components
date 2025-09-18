idf_build_get_property(target IDF_TARGET)

set(srcs "esp_repl.c")

idf_component_register(
                    SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES test_esp_linenoise test_esp_commands)
