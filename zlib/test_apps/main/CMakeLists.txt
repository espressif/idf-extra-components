set(priv_requires unity)
if(NOT IDF_TARGET STREQUAL "linux")
    # The PSRAM placement tests run only where a PSRAM can exist.
    list(APPEND priv_requires esp_psram)
endif()

idf_component_register(
    SRCS "test_main.c"
         "test_zlib_roundtrip.c"
         "test_zlib_alloc.c"
    PRIV_REQUIRES ${priv_requires}
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)
