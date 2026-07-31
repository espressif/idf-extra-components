include(${CMAKE_CURRENT_LIST_DIR}/../../esp_schedule_variables.cmake)
set(_srcs "test_app_main.c" "test_esp_schedule.c")
# The variables file covers the portable half only; the ESP glue directory holds
# the *_impl.h headers that glue_log.h and glue_mem.h include.
list(APPEND ESP_SCHEDULE_PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../glue/esp")
idf_component_register(SRCS ${_srcs}
                    PRIV_INCLUDE_DIRS ${ESP_SCHEDULE_PRIV_INCLUDE_DIRS}
                    PRIV_REQUIRES ${ESP_SCHEDULE_PRIV_REQUIRES} "unity" "nvs_flash" "esp_netif"
                    WHOLE_ARCHIVE
                    )
