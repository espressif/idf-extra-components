include(${CMAKE_CURRENT_LIST_DIR}/esp_schedule_variables.cmake)

# The ESP-IDF port. esp_schedule_default.c is what binds it to
# esp_schedule_init(); the port/esp sources are reachable only from there.
#
# CONFIG_ESP_SCHEDULE_DISABLE_DEFAULT_PORT=y drops them from the build outright.
# That is a stronger guarantee than the link-time one: with the default port
# compiled in, an application that only calls esp_schedule_init_with_config()
# still gets it stripped, but the objects are built either way.
if(NOT CONFIG_ESP_SCHEDULE_DISABLE_DEFAULT_PORT)
    list(APPEND ESP_SCHEDULE_SRCS "src/esp_schedule_default.c"
                                  "port/esp/log.c"
                                  "port/esp/mem.c"
                                  "port/esp/nvs.c"
                                  "port/esp/time.c"
                                  "port/esp/timer.c")
endif()

# Declared unconditionally, unlike the sources above. ESP-IDF resolves component
# requirements in an early expansion pass that runs before the configuration
# exists, so a CONFIG_-guarded PRIV_REQUIRES is silently ignored and the port
# sources fail to find their headers. Requiring these always is harmless: with
# the default port disabled nothing references them, so nothing is linked.
list(APPEND ESP_SCHEDULE_PRIV_REQUIRES "nvs_flash" "esp_netif")

idf_component_register(SRCS ${ESP_SCHEDULE_SRCS}
                       INCLUDE_DIRS ${ESP_SCHEDULE_INCLUDE_DIRS}
                       PRIV_INCLUDE_DIRS ${ESP_SCHEDULE_PRIV_INCLUDE_DIRS}
                       PRIV_REQUIRES ${ESP_SCHEDULE_PRIV_REQUIRES})
