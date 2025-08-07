idf_component_register(SRCS "test_esp_commands.c" "test_main.c"
                    PRIV_INCLUDE_DIRS "." "include"
                    PRIV_REQUIRES unity
                    WHOLE_ARCHIVE)
