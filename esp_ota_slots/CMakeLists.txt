idf_component_register(SRCS "src/esp_ota_slots.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_partition
                       PRIV_REQUIRES nvs_flash app_update log)

if(COMMAND idf_define_esp_err_codes)
    idf_define_esp_err_codes(HEADERS include/esp_ota_slots.h)
endif()
