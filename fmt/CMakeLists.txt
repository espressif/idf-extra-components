idf_component_register( )

add_subdirectory(fmt)

target_link_libraries(${COMPONENT_LIB} INTERFACE fmt::fmt)

