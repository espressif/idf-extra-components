idf_component_register(SRCS "src/esp_custom_part_ota.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_partition app_update nvs_flash)
