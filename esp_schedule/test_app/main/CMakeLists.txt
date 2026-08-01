include(${CMAKE_CURRENT_LIST_DIR}/../../esp_schedule_variables.cmake)
set(_srcs "test_app_main.c" "test_esp_schedule.c")
# The tests reach into esp_schedule_internal.h, so they need the component's own
# private include dirs. The ESP-IDF port tables that test_app_main.c installs are
# declared in esp_schedule_esp_port.h, which is public, so nothing extra is
# needed for those.
idf_component_register(SRCS ${_srcs}
                    PRIV_INCLUDE_DIRS ${ESP_SCHEDULE_PRIV_INCLUDE_DIRS}
                    PRIV_REQUIRES ${ESP_SCHEDULE_PRIV_REQUIRES} "unity" "nvs_flash"
                    WHOLE_ARCHIVE
                    )
