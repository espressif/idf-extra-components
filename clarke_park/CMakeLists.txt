include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(srcs "src/clarke_park_f.c" "src/clarke_park_iq.c")
set(requires "iqmath")

# The CORDIC backend replaces the IQmath backend for the Park trigonometric
# functions. It is only compiled on chips that have the CORDIC accelerator
# (SOC_CORDIC_SUPPORTED, currently ESP32-S3R) when the Kconfig option is on.
# Note: the component's own Kconfig values are not yet available while the
# component requirements are resolved, so this only selects the source files.
if(CONFIG_CLARKE_PARK_USE_CORDIC_HW AND CONFIG_SOC_CORDIC_SUPPORTED)
    list(APPEND srcs "src/clarke_park_cordic.c")
endif()

# The CORDIC driver component was introduced in esp-idf v6.1 (along with the
# ESP32-S3R target). clarke_park.h exposes the CORDIC engine handle (via
# clarke_park_cordic_get_engine) when the CORDIC backend is enabled, so the
# driver must be a public dependency. It is declared unconditionally for
# IDF >= 6.1 because requirements are resolved before this component's own
# Kconfig options are available; on targets without CORDIC the driver
# component simply has no sources.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.1")
    list(APPEND requires "esp_driver_cordic")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES ${requires})
