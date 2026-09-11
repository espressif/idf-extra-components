idf_component_register(SRCS "test_app_main.c"
                            "pid_ctrl_test.c"
                            "pid_ctrl_test.cpp"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity
                       WHOLE_ARCHIVE)
