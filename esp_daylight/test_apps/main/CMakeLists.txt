idf_component_register(SRCS "test_app_main.c"
                            "test_esp_daylight.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity esp_daylight
                       WHOLE_ARCHIVE)
