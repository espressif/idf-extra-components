idf_component_register(SRCS
                        "linenoise/linenoise.c"
                        "src/esp_linenoise.c"
                        "src/esp_linenoise_internals.c"
                       PRIV_INCLUDE_DIRS
                        "private_include"
                       PRIV_REQUIRES vfs
                       INCLUDE_DIRS
                        "include"
                        "."
)

