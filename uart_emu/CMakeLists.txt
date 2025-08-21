set(srcs)
set(public_requires)

if(CONFIG_SOC_RMT_SUPPORTED)
    list(APPEND srcs "src/uart_emu_rmt.c" "src/uart_emu_rmt_encoder.c")
endif()

# Starting from esp-idf v5.3, the drivers are moved to separate components
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND public_requires "esp_driver_rmt")
else()
    list(APPEND public_requires "driver")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES ${public_requires})