set(srcs
    "descriptors_control.c"
    "tinyusb.c"
    "usb_descriptors.c"
    )

if(NOT CONFIG_TINYUSB_NO_DEFAULT_TASK)
    list(APPEND srcs "tusb_tasks.c")
endif() # CONFIG_TINYUSB_NO_DEFAULT_TASK

if(CONFIG_TINYUSB_CDC_ENABLED)
    list(APPEND srcs
        "cdc.c"
        "tusb_cdc_acm.c"
        )
    if(CONFIG_VFS_SUPPORT_IO)
        list(APPEND srcs
            "tusb_console.c"
            "vfs_tinyusb.c"
            )
    endif() # CONFIG_VFS_SUPPORT_IO
endif() # CONFIG_TINYUSB_CDC_ENABLED

if(CONFIG_TINYUSB_MSC_ENABLED)
    list(APPEND srcs
        tusb_msc_storage.c
        )
endif() # CONFIG_TINYUSB_MSC_ENABLED

if(CONFIG_TINYUSB_NET_MODE_NCM)
    list(APPEND srcs
         tinyusb_net.c
         )
endif() # CONFIG_TINYUSB_NET_MODE_NCM

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "include_private"
                       PRIV_REQUIRES usb
                       REQUIRED_IDF_TARGETS esp32s2 esp32s3
                       )

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

# Pass tusb_config.h from this component to TinyUSB
idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "include")

# Link required libraries
# This must be done after idf_component_register, because Kconfig values are not resolved before it
if(CONFIG_TINYUSB_MSC_ENABLED)
    idf_component_get_property(fatfs fatfs COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${fatfs})
endif()

if((CONFIG_TINYUSB_CDC_ENABLED) AND (CONFIG_VFS_SUPPORT_IO))
    idf_component_get_property(vfs vfs COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PUBLIC ${vfs})
endif()
