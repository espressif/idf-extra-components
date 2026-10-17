set(srcs "src/clarke_park_f.c"
         "src/clarke_park_iq.c"
         "src/clarke_park_sincos.c"
         "src/clarke_park_cordic.c")

set(priv_requires)

# CORDIC driver exists since IDF v6.1. The 6.2 gate is temporary: release/v6.1
# is still missing driver fixes (shared acquire/release, thread-safe polling, and
# the iteration-count semantics the Q15 backend relies on) that have not been
# backported yet. Restore 6.1 here once that lands.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.2")
    list(APPEND priv_requires "esp_driver_cordic")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
