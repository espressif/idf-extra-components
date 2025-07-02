idf_component_register(SRCS "test_esp_linenoise_get_set.c"
                            "test_esp_linenoise_behavioral.c"
                            "test_linenoise_legacy.c"
                            "test_utils.c"
                            "test_main.c"
                    PRIV_INCLUDE_DIRS "." "include"
                    PRIV_REQUIRES unity
                    WHOLE_ARCHIVE)
