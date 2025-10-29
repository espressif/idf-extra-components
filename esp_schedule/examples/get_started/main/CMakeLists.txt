set(supported_targets "esp32" "esp32s2" "esp32s3" "esp32c2" "esp32c3" "esp32c5" "esp32c6" "esp32c61")

# Redirect to a stub if target is not supported
if(CONFIG_IDF_TARGET IN_LIST supported_targets)
    set(srcs "esp_schedule_example_main.c"
            "network/app_network.c"
        )
    set(priv_include_dirs "network")
else()
    message(WARNING "Target ${CONFIG_IDF_TARGET} is not supported. Redirecting to stub example.")
    set(srcs "esp_schedule_example_stub.c")
    set(priv_include_dirs "")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       PRIV_REQUIRES esp_schedule nvs_flash esp_netif esp_event esp_wifi)
