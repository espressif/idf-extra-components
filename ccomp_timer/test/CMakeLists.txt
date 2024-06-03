idf_build_get_property(arch IDF_TARGET_ARCH)

set(priv_requires esp_timer ccomp_timer unity)
if("${arch}" STREQUAL "xtensa")
    list(APPEND priv_requires perfmon)
endif()

idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_requires})
