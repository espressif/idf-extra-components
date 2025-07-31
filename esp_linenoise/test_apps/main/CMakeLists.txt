idf_component_register(SRCS "test_linenoise_get_set.c" "test_linenoise_behavioral.c" "test_main.c"
                    PRIV_INCLUDE_DIRS "." "include"
                    PRIV_REQUIRES unity
                    WHOLE_ARCHIVE)
