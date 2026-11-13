idf_component_register(SRCS "test_main.c" "test_common.c" "test_compliance.c" "test_appendix_b.c"
                            "test_slots_persist.c"
                            "vectors/appendix_b.c" "vectors/spec_key.c" "vectors/compliance_vectors.c"
                       INCLUDE_DIRS "." "vectors"
                       PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../private_include"
                       EMBED_FILES "vectors/compliance.bin"
                       PRIV_REQUIRES unity nvs_flash esp_partition esp_event app_update mbedtls
                       WHOLE_ARCHIVE)
