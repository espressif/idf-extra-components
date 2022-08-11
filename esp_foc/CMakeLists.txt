idf_build_get_property(target IDF_TARGET)

set(requires driver freertos esp_system esp_timer)  
set(srcs "esp_foc.c" "esp_foc_consts.c")
set(includes "include" "drivers")

list(APPEND srcs   "drivers/inverter_3pwm_ledc.c"
                    "drivers/rotor_sensor_as5600.c"
                    "drivers/rotor_sensor_dummy.c"
                    "drivers/os_interface.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${includes}"
                       PRIV_REQUIRES "${requires}")