idf_component_register(SRCS "test_app_main.c" "pid_ctrl_test.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES unity pid_ctrl
                    WHOLE_ARCHIVE)
