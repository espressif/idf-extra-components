set(component_srcs "src/esp_schedule.c")

# Conditionally include NVS persistence sources
if(CONFIG_ESP_SCHEDULE_ENABLE_NVS)
    list(APPEND component_srcs "src/esp_schedule_nvs.c")
endif()

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       PRIV_REQUIRES esp_netif)

# nvs_flash is only needed when NVS persistence is enabled
if(CONFIG_ESP_SCHEDULE_ENABLE_NVS)
    idf_component_optional_requires(PRIVATE nvs_flash)
endif()
