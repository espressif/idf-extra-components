include(${CMAKE_CURRENT_LIST_DIR}/../../esp_schedule_variables.cmake)
set(_srcs "test_app_main.c" "test_esp_schedule.c")
idf_component_register(SRCS ${_srcs}
                    PRIV_INCLUDE_DIRS ${ESP_SCHEDULE_PRIV_INCLUDE_DIRS}
                    PRIV_REQUIRES ${ESP_SCHEDULE_PRIV_REQUIRES} "unity" "nvs_flash"
                    WHOLE_ARCHIVE
                    )
