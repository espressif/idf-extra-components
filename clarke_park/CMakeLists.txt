set(srcs "src/clarke_park_f.c"
         "src/clarke_park_iq.c"
         "src/clarke_park_sincos.c"
         "src/clarke_park_cordic.c")

set(priv_requires)

# esp_driver_cordic component is introduced since IDF v6.1
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.1")
    list(APPEND priv_requires "esp_driver_cordic")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
