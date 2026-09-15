idf_component_register(SRCS "test_app_main.c"
                            "clarke_park_test.c"
                            "clarke_park_test.cpp"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity
                       WHOLE_ARCHIVE)
