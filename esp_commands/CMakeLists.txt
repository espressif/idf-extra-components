idf_build_get_property(target IDF_TARGET)

set(srcs "esp_commands.c"
         "esp_commands_helpers.c")

idf_component_register(
                    SRCS ${srcs}
                    INCLUDE_DIRS include
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES heap
                    LDFRAGMENTS linker.lf)
