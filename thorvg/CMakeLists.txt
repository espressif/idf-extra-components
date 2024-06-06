# Set a name for libthorvg library
set(TVG_LIB libthorvg)
set(TVG_INC_DIR thorvg/src/bindings/capi)
set(TVG_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}/thorvg")
set(TVG_BUILD_DIR "${CMAKE_BINARY_DIR}/thorvg_build")
set(TVG_CROSS_CFG "${CMAKE_BINARY_DIR}/thorvg_build/cross_file.txt")
idf_build_get_property(python PYTHON)

idf_component_register(SRCS dummy.c
                    INCLUDE_DIRS "${TVG_INC_DIR}")

include(ExternalProject)

if(NOT CMAKE_CXX_FLAGS STREQUAL "")
    string(REGEX MATCHALL "-[^ ]+" match_list "${CMAKE_CXX_FLAGS}")
    set(MESON_CXX_FLAGS "")

    list(LENGTH match_list match_list_length)
    math(EXPR last_index "${match_list_length}-1")

    foreach(index RANGE ${last_index})
        list(GET match_list ${index} match)
        string(APPEND MESON_CXX_FLAGS "'${match}'")
        if(NOT ${index} EQUAL ${last_index})
            string(APPEND MESON_CXX_FLAGS ", ")
        endif()
    endforeach()
else()
    string(APPEND MESON_CXX_FLAGS "''")
endif()

if(NOT CMAKE_EXE_LINKER_FLAGS STREQUAL "")
    string(REGEX MATCHALL "-[^ ]+" match_list "${CMAKE_EXE_LINKER_FLAGS}")
    set(MESON_LINKER_FLAGS "")

    list(LENGTH match_list match_list_length)
    math(EXPR last_index "${match_list_length}-1")

    foreach(index RANGE ${last_index})
        list(GET match_list ${index} match)
        string(APPEND MESON_LINKER_FLAGS "'${match}'")
        if(NOT ${index} EQUAL ${last_index})
            string(APPEND MESON_LINKER_FLAGS ", ")
        endif()
    endforeach()
else()
    string(APPEND MESON_LINKER_FLAGS "''")
endif()

function(replace_tool_name last_name output_variable)
    set(old_string "${_CMAKE_TOOLCHAIN_PREFIX}g++")
    set(new_string "${_CMAKE_TOOLCHAIN_PREFIX}${last_name}")
    string(REPLACE "${old_string}" "${new_string}" ${output_variable} "${CMAKE_CXX_COMPILER}")
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

replace_tool_name("ar" ar_path)
replace_tool_name("as" as_path)
replace_tool_name("ranlib" ranlib_path)
replace_tool_name("ld" ld_path)
replace_tool_name("strip" srtip_path)

file(WRITE ${TVG_CROSS_CFG} "
[binaries]
cpp = '${CMAKE_CXX_COMPILER}'
ar = '${ar_path}'
as = '${as_path}'
ranlib = '${ranlib_path}'
ld = '${ld_path}'
strip = '${srtip_path}'

[built-in options]
cpp_args = ['-D_GNU_SOURCE','-D__linux__',${MESON_CXX_FLAGS}]
")

if(NOT MESON_LINKER_FLAGS STREQUAL "''")
    file(APPEND ${TVG_CROSS_CFG} "
cpp_link_args = [${MESON_LINKER_FLAGS}]
")
endif()

file(APPEND ${TVG_CROSS_CFG} "
[host_machine]
system = 'freertos'
cpu_family = '${CMAKE_SYSTEM}'
cpu = 'esp'
endian = 'little'
")

execute_process(
    COMMAND ${python} -m pip install -U meson
    RESULT_VARIABLE pip_result
    OUTPUT_VARIABLE pip_output
)

if(NOT pip_result EQUAL 0)
    message(FATAL_ERROR "Failed to install or update meson using pip.")
endif()

ExternalProject_Add(${TVG_LIB}_target
    PREFIX ${TVG_BUILD_DIR}
    SOURCE_DIR ${TVG_SUBDIR}
    BINARY_DIR ${TVG_BUILD_DIR}
    CONFIGURE_COMMAND meson setup ${TVG_BUILD_DIR} ${TVG_SUBDIR}
    --cross-file ${TVG_CROSS_CFG}
    -Dbindings=capi
    -Dextra=
    -Ddefault_library=static
    -Dthreads=false
    BUILD_COMMAND ninja -C ${TVG_BUILD_DIR}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${TVG_BUILD_DIR}/src/${TVG_LIB}.a
)

add_prebuilt_library(${TVG_LIB} ${TVG_BUILD_DIR}/src/${TVG_LIB}.a PRIV_REQUIRES esp_rom)
add_dependencies(${TVG_LIB} ${TVG_LIB}_target)

target_link_libraries(${COMPONENT_LIB} INTERFACE ${TVG_LIB})
