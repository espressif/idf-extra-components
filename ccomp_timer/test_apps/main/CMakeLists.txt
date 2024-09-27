idf_build_get_property(arch IDF_TARGET_ARCH)

set(priv_requires esp_timer unity)
if("${arch}" STREQUAL "xtensa")
    list(APPEND priv_requires perfmon)
endif()

idf_component_register(SRCS "ccomp_timer_test.c"
                            "ccomp_timer_test_api.c"
                            "ccomp_timer_test_data.c"
                            "ccomp_timer_test_inst.c"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_requires}
                       WHOLE_ARCHIVE)
