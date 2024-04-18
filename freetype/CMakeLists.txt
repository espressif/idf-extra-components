idf_component_register()

set(CMAKE_SYSTEM_IGNORE_PATH "/usr/include;/usr/lib/x86_64-linux-gnu")
set(BUILD_SHARED_LIBS OFF)

function(install)
endfunction()

function(export)
endfunction()

add_subdirectory(freetype output)
target_compile_options(freetype PRIVATE "-Wno-dangling-pointer")

target_link_libraries(${COMPONENT_LIB} INTERFACE freetype)