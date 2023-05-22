set(sources src/msc_scsi_bot.c
            src/diskio_usb.c
            src/msc_host.c
            src/msc_host_vfs.c)

idf_component_register( SRCS ${sources}
                        INCLUDE_DIRS include include/usb # 'include' is here for backwards compatibility
                        PRIV_INCLUDE_DIRS private_include
                        REQUIRES usb fatfs )

# We access packeted USB string descriptor via pointers. The memory used for storing the descritptors
# allows unaligned access, so this is not an issue
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-address-of-packed-member)
