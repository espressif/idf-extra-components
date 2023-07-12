idf_component_register( SRCS "hid_host.c"
                        INCLUDE_DIRS "include"
					    PRIV_REQUIRES usb )

# We access packeted USB string descriptor via pointers. The memory used for storing the descritptors
# allows unaligned access, so this is not an issue
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-address-of-packed-member)