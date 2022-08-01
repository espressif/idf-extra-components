configure_file(${CMAKE_CURRENT_LIST_DIR}/libuvc/include/libuvc/libuvc_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/libuvc/libuvc_config.h
               @ONLY)

set(LIBUVC_SOURCES libuvc/src/ctrl.c
                   libuvc/src/ctrl-gen.c
                   libuvc/src/device.c
                   libuvc/src/diag.c
                   libuvc/src/frame.c
                   libuvc/src/init.c
                   libuvc/src/misc.c
                   libuvc/src/stream.c)

idf_component_register(
    SRCS ${LIBUVC_SOURCES} src/descriptor.c src/libusb_adapter.c
    INCLUDE_DIRS include libuvc/include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES usb pthread)

set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/libuvc/src/device.c PROPERTIES COMPILE_FLAGS -Wno-implicit-fallthrough)
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/libuvc/src/stream.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable)
    set_source_files_properties(
        ${CMAKE_CURRENT_LIST_DIR}/libuvc/src/diag.c PROPERTIES COMPILE_FLAGS -Wno-format)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LIBUVC_NUM_TRANSFER_BUFS=4)
target_include_directories(${COMPONENT_LIB} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include/)
