set(includes_private
    "include_private"
    )

set(includes_public
    "include"
    )

set(srcs
    "descriptors_control.c"
    "tinyusb.c"
    "usb_descriptors.c"
    )

set(priv_requires
    usb
    vfs
    esp_ringbuf
    )

if(NOT CONFIG_TINYUSB_NO_DEFAULT_TASK)
    list(APPEND srcs "tusb_tasks.c")
endif() # CONFIG_TINYUSB_NO_DEFAULT_TASK

if(CONFIG_TINYUSB_CDC_ENABLED)
    list(APPEND srcs
        "cdc.c"
        "tusb_cdc_acm.c"
        "tusb_console.c"
        "vfs_tinyusb.c"
        )
endif() # CONFIG_TINYUSB_CDC_ENABLED

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes_public}
                       PRIV_INCLUDE_DIRS ${includes_private}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRED_IDF_TARGETS esp32s2 esp32s3
                       )

# Pass tusb_config.h from this component to TinyUSB
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "include")
