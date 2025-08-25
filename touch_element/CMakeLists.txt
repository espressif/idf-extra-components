idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return() # This component is not supported by the POSIX/Linux simulator
endif()

idf_component_register(SRCS "touch_element.c"
        "touch_button.c"
        "touch_slider.c"
        "touch_matrix.c"
        "touch_sensor_legacy_hal.c"
        INCLUDE_DIRS include
        PRIV_REQUIRES esp_timer esp_pm esp_driver_gpio)
