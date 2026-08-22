idf_component_register(
    SRCS "test_main.c"
         "test_lz4_roundtrip.c"
         "test_lz4_benchmark.c"
    PRIV_REQUIRES unity esp_psram esp_timer
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)
