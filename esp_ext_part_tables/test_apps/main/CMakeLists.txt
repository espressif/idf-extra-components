set(priv_requires "unity")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(APPEND priv_requires "esp_blockdev")
endif()

idf_component_register(SRCS "test_esp_ext_part.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ${priv_requires}
                    WHOLE_ARCHIVE)
