# The tests exercise the component's internal helpers (esp_schedule_internal.h),
# so the component's private src/ directory is added to the include path. The
# esp_schedule dependency itself is pulled in via main/idf_component.yml.
set(_srcs "test_app_main.c" "test_esp_schedule.c")
idf_component_register(SRCS ${_srcs}
                    PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../src"
                    PRIV_REQUIRES "esp_daylight" "unity" "nvs_flash" "esp_netif"
                    WHOLE_ARCHIVE
                    )
