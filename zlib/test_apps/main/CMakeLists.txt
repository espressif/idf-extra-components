idf_component_register(
    SRCS "test_main.c"
         "test_zlib_roundtrip.c"
         "test_zlib_alloc.c"
    PRIV_REQUIRES unity esp_psram
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)
