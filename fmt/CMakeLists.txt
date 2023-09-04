idf_component_register( )

set(FMT_INSTALL OFF)
add_subdirectory(fmt)

target_link_libraries(${COMPONENT_LIB} INTERFACE fmt::fmt)

