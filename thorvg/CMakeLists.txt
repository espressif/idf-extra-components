# Set a name for libthorvg library
set(TVG_LIB libthorvg)
set(TVG_INC_DIR thorvg/src/bindings/capi)
set(TVG_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}/thorvg")
set(TVG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/thorvg_build")
set(TVG_CROSS_CFG "${CMAKE_CURRENT_BINARY_DIR}/thorvg_build/cross_file.txt")

set(TVG_LOADERS_OPTION "")

if(CONFIG_THORVG_LOTTIE_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "lottie")
endif()

if(CONFIG_THORVG_SVG_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "svg")
endif()

if(CONFIG_THORVG_TVG_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "tvg")
endif()

if(CONFIG_THORVG_PNG_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "png")
endif()

if(CONFIG_THORVG_JPEG_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "jpg")
endif()

if(CONFIG_THORVG_WEBP_LOADER_SUPPORT)
    list(APPEND TVG_LOADERS_OPTION "webp")
endif()

list(JOIN TVG_LOADERS_OPTION "," TVG_LOADERS_OPTION)

if(CONFIG_THORVG_LOG_ENABLED)
    set(TVG_LOG "true")
else()
    set(TVG_LOG "false")
endif()

if(CONFIG_THORVG_THREAD_ENABLED)
    set(TVG_THREADS "true")
else()
    set(TVG_THREADS "false")
endif()

idf_component_register(
    SRCS dummy.c
    INCLUDE_DIRS "${TVG_INC_DIR}"
    PRIV_REQUIRES pthread)


# Expands CMake flags that may contain response files (@"file" syntax) into a list.
# ESP-IDF v6 uses @"file" syntax for response files, but Meson doesn't support this.
# We need to expand the response file contents because:
# 1. CMake expands @"file" internally, but Meson passes it literally to GCC
# 2. GCC only supports @file (without quotes), not @"file"
# 3. We can't just remove quotes because paths may contain spaces (especially on Windows)
function(expand_response_file input_flags output_var)
    set(flags ${input_flags})
    if(flags MATCHES "@\"(.+)\"")
        set(response_file "${CMAKE_MATCH_1}")
        if(EXISTS "${response_file}")
            file(STRINGS "${response_file}" flags)
        else()
            set(flags "")
        endif()
    else()
        separate_arguments(flags)
    endif()
    list(FILTER flags EXCLUDE REGEX "^$")
    set(${output_var} ${flags} PARENT_SCOPE)
endfunction()

expand_response_file("${CMAKE_CXX_FLAGS}" compiler_args)
expand_response_file("${CMAKE_EXE_LINKER_FLAGS}" linker_args)

message(STATUS "CMAKE_CXX_FLAGS: ${compiler_args}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${linker_args}")

# Add pthread library path to the linker flags if threads are enabled
if(TVG_THREADS STREQUAL "true")
    idf_component_get_property(pthread_lib "pthread" COMPONENT_LIB GENERATOR_EXPRESSION)
    set(pthread_lib_path_expr "$<TARGET_FILE_DIR:${pthread_lib}>")
    list(APPEND linker_args "-L${pthread_lib_path_expr}")
    set(TVG_REQUIRES pthread)
endif()

# Convert the list of arguments (arg;arg;arg) to a string "'arg', 'arg', 'arg'"
list(JOIN compiler_args "', '" MESON_CXX_FLAGS)
set(MESON_CXX_FLAGS "'${MESON_CXX_FLAGS}'")
list(JOIN linker_args "', '" MESON_LINKER_FLAGS)
set(MESON_LINKER_FLAGS "'${MESON_LINKER_FLAGS}'")

# Please note, xtensa is not recognized by Meson, but "riscv32" is a safe generic option
# Always use "riscv32" as cpu_family in cross_file.txt to avoid Meson warnings
set(cpu_family "riscv32")

# Two-step generation of cross_file.txt to expand CMake variables...
configure_file(${CMAKE_CURRENT_LIST_DIR}/cross_file.txt.in ${TVG_CROSS_CFG}.tmp)
# ...and then expand generator expressions
file(GENERATE OUTPUT ${TVG_CROSS_CFG} INPUT ${TVG_CROSS_CFG}.tmp)

include(ExternalProject)

ExternalProject_Add(${TVG_LIB}_target
    PREFIX ${TVG_BUILD_DIR}
    SOURCE_DIR ${TVG_SUBDIR}
    BINARY_DIR ${TVG_BUILD_DIR}
    CONFIGURE_COMMAND meson setup ${TVG_BUILD_DIR} ${TVG_SUBDIR}
    --cross-file ${TVG_CROSS_CFG}
    -Dbindings=capi
    -Dextra=
    -Ddefault_library=static # build static library
    -Db_staticpic=false # no -fPIC
    -Dthreads=${TVG_THREADS}
    -Dlog=${TVG_LOG} # allow log output
    -Dloaders=${TVG_LOADERS_OPTION}   # Pass the loaders option to Meson
    BUILD_COMMAND ninja -C ${TVG_BUILD_DIR}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${TVG_BUILD_DIR}/src/${TVG_LIB}.a
)

add_prebuilt_library(${TVG_LIB} ${TVG_BUILD_DIR}/src/${TVG_LIB}.a PRIV_REQUIRES ${TVG_REQUIRES})
add_dependencies(${TVG_LIB} ${TVG_LIB}_target)

if(TVG_THREADS STREQUAL "true")
    add_dependencies(${TVG_LIB}_target idf::pthread)
endif()

target_link_libraries(${COMPONENT_LIB} INTERFACE ${TVG_LIB})
