set(srcs)

set(include "include" "interface")

list(APPEND srcs "src/sccb.c")

if(CONFIG_SOC_I2C_SUPPORTED)
    list(APPEND srcs "sccb_i2c/src/sccb_i2c.c")
endif()

list(APPEND include "sccb_i2c/include")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include}
                       REQUIRES esp_driver_i2c
                      )
