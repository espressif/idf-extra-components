set(srcs "src/bdc_motor.c")

if(CONFIG_SOC_MCPWM_SUPPORTED)
    list(APPEND srcs "src/bdc_motor_mcpwm_impl.c")
endif()

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(priv_requires "esp_driver_mcpwm")
else()
    set(priv_requires "driver")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include" "interface"
                       PRIV_REQUIRES ${priv_requires})
