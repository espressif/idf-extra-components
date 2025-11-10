
idf_component_register(SRCS "test_esp_cli.c" "test_main.c"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES unity
                    WHOLE_ARCHIVE)
