set(srcs "src/clarke_park_f.c"
         "src/clarke_park_iq.c"
         "src/clarke_park_sincos.c"
         "src/clarke_park_cordic.c")

set(priv_requires)

# CORDIC driver exists since IDF v6.1. The 6.2 gate is temporary: release/v6.1
# is still missing a driver fix (shared acquire/release + thread-safe polling)
# that has not been backported yet. Restore 6.1 here once that lands.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.2")
    list(APPEND priv_requires "esp_driver_cordic")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
