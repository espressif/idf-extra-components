set(priv_requires unity)
if(NOT IDF_TARGET STREQUAL "linux")
    # The PSRAM placement checks run only where a PSRAM can exist.
    list(APPEND priv_requires esp_psram)
endif()

idf_component_register(
    SRCS test_libpng.c test_main.c
    PRIV_REQUIRES ${priv_requires}
    WHOLE_ARCHIVE
    EMBED_FILES "in.png" "out.pgm")
