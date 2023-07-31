idf_component_register()

set(BUILD_SHARED_LIBS OFF)
option(BUILD_TESTING OFF)

function(install)
endfunction()

function(export)
endfunction()

add_subdirectory(freetype output)
target_compile_options(freetype PRIVATE "-Wno-dangling-pointer")

target_link_libraries(${COMPONENT_LIB} INTERFACE freetype)