include(${CMAKE_CURRENT_LIST_DIR}/esp_schedule_variables.cmake)

# Include ESP-IDF specific glue sources and include directories
list(APPEND ESP_SCHEDULE_SRCS "src/esp_schedule_nvs.c"
                              "glue/esp/nvs.c"
                              "glue/esp/time.c"
                              "glue/esp/timer.c")
list(APPEND ESP_SCHEDULE_INCLUDE_DIRS "include/esp")
list(APPEND ESP_SCHEDULE_PRIV_INCLUDE_DIRS "glue/esp")
list(APPEND ESP_SCHEDULE_PRIV_REQUIRES "nvs_flash" "esp_netif")

idf_component_register(SRCS ${ESP_SCHEDULE_SRCS}
                       INCLUDE_DIRS ${ESP_SCHEDULE_INCLUDE_DIRS}
                       PRIV_INCLUDE_DIRS ${ESP_SCHEDULE_PRIV_INCLUDE_DIRS}
                       PRIV_REQUIRES ${ESP_SCHEDULE_PRIV_REQUIRES})
