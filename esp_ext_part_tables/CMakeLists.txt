set(srcs "src/esp_ext_part_tables.c"
         "src/esp_mbr.c"
         "src/esp_mbr_utils.c")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    REQUIRES "log" "esp_common")
