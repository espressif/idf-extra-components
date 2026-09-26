# ThorVG builds with Meson. This component compiles no sources of its own:
# ExternalProject runs setup -> compile -> install into an install prefix,
# The installed capi headers and static library are exported to the IDF build.

idf_component_register()

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# Expand a compiler/linker flag string that may be a response file
# (@"file" syntax, used by ESP-IDF v6 toolchains) into a CMake list of
# single arguments:
#   1. CMake expands @"file" internally, but Meson would pass it literally to
#      GCC, and GCC only understands unquoted @file.
#   2. Response files may contain quoted flags ("-specs=path") which confuse
#      Meson, so strip the surrounding double quotes after expansion.
# Non-response input is split on unquoted whitespace (native parsing).
function(thorvg_expand_response_file input output_var)
    set(flags ${input})
    if(flags MATCHES "@\"(.+)\"")
        set(response_file "${CMAKE_MATCH_1}")
        if(EXISTS "${response_file}")
            file(STRINGS "${response_file}" flags)
        else()
            message(FATAL_ERROR
                "ESP-IDF compiler response file does not exist: ${response_file}. "
                "ThorVG cannot be built safely without the complete toolchain flags.")
        endif()
    else()
        separate_arguments(flags)
    endif()
    list(FILTER flags EXCLUDE REGEX "^$")
    list(TRANSFORM flags REPLACE "^\"(.*)\"$" "\\1")
    set(${output_var} ${flags} PARENT_SCOPE)
endfunction()

# Serialize a CMake list as a Meson string-array option value for the command
# line, e.g. ['flag one', '-DFOO=bar']. Meson parses the bracket form with
# literal_eval, so commas inside flags (e.g. -Wl,...) are safe; embedded
# quotes and backslashes are escaped so the round trip is loss-free.
function(thorvg_join_meson_array output_var)
    set(items "")
    foreach(arg IN LISTS ARGN)
        string(REPLACE "\\" "\\\\" arg "${arg}")
        string(REPLACE "'" "\\'" arg "${arg}")
        list(APPEND items "'${arg}'")
    endforeach()
    list(JOIN items ", " joined)
    set(${output_var} "[${joined}]" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(THORVG_LIB libthorvg)
set(THORVG_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}/thorvg")
set(THORVG_EP_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/thorvg_ep")
set(THORVG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/thorvg_meson_build")
set(THORVG_CROSS_CFG "${THORVG_BUILD_DIR}/cross_file.txt")
set(THORVG_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/thorvg_install")
set(THORVG_STATIC_LIB "${THORVG_INSTALL_DIR}/lib/libthorvg-1.a")
set(THORVG_INC_DIR "${THORVG_INSTALL_DIR}/include/thorvg-1")

# Headers land in the install tree only after the install step. Create the
# directory now so INTERFACE_INCLUDE_DIRECTORIES is valid at configure time.
file(MAKE_DIRECTORY "${THORVG_INC_DIR}")

# ---------------------------------------------------------------------------
# sdkconfig -> Meson options
# ---------------------------------------------------------------------------

# Kconfig suffix -> Meson loader name (JPEG ships as Meson's 'jpg', etc.)
set(THORVG_LOADERS "")
foreach(map_entry
        "LOTTIE=lottie"
        "SVG=svg"
        "PNG=png"
        "JPEG=jpg"
        "WEBP=webp"
        "OTF=otf"
        "TTF=ttf")
    string(REPLACE "=" ";" map_entry "${map_entry}")
    list(GET map_entry 0 kconfig_suffix)
    list(GET map_entry 1 meson_name)
    if(CONFIG_THORVG_${kconfig_suffix}_LOADER_SUPPORT)
        list(APPEND THORVG_LOADERS "${meson_name}")
    endif()
endforeach()
list(JOIN THORVG_LOADERS "," THORVG_LOADERS_OPTION)

if(CONFIG_THORVG_LOG_ENABLED)
    set(THORVG_LOG "true")
else()
    set(THORVG_LOG "false")
endif()

# thorvg_capi.h uses `const Tvg_Paint` return types and `foo()` prototypes.
# IDF compiles user C files with -Werror, so suppress those here.
target_compile_options(${COMPONENT_LIB} INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-Wno-ignored-qualifiers>
    $<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>
)

# ---------------------------------------------------------------------------
# Harvest the flags a normal IDF C++ component would see, for Meson
# ---------------------------------------------------------------------------

thorvg_expand_response_file("${CMAKE_CXX_FLAGS}" compiler_args)
thorvg_expand_response_file("${CMAKE_EXE_LINKER_FLAGS}" linker_args)

# CMAKE_CXX_FLAGS / CMAKE_EXE_LINKER_FLAGS are only the toolchain @cxxflags
# and @ldflags files. Fold in the project-level IDF properties so ThorVG
# matches a normal C++ component: COMPILE_OPTIONS + CXX_COMPILE_OPTIONS +
# COMPILE_DEFINITIONS on compile, LINK_OPTIONS on link.
# C_COMPILE_OPTIONS / ASM_COMPILE_OPTIONS are skipped: meson.build is cpp-only.
# INCLUDE_DIRECTORIES is only the sdkconfig.h directory; ThorVG does not use it.
idf_build_get_property(thorvg_compile_options COMPILE_OPTIONS)
idf_build_get_property(thorvg_cxx_compile_options CXX_COMPILE_OPTIONS)
list(APPEND compiler_args ${thorvg_compile_options} ${thorvg_cxx_compile_options})
idf_build_get_property(thorvg_compile_definitions COMPILE_DEFINITIONS)
foreach(def IN LISTS thorvg_compile_definitions)
    if(NOT def MATCHES "^-D")
        set(def "-D${def}")
    endif()
    list(APPEND compiler_args "${def}")
endforeach()
idf_build_get_property(thorvg_link_options LINK_OPTIONS)
list(APPEND linker_args ${thorvg_link_options})

# ThorVG is third-party and already sets its own warning flags. Do not inherit
# IDF's -Wall/-Wextra/-Werror, or -std=gnu++26 (upstream is C++14).
list(FILTER compiler_args EXCLUDE REGEX "^-W")
list(FILTER compiler_args EXCLUDE REGEX "^-std=")

# Meson warning_level=1 still adds -Wall. Keep this after the -W filter so
# format warnings in upstream ThorVG stay suppressed.
list(APPEND compiler_args "-Wno-format")

# Meson applies cpp_args twice during its cross-compiler sanity check. GCC
# cannot load a specs file twice because it redefines compiler driver specs.
# Keep -specs in cpp_args: picolibc.specs also configures preprocessing and
# compilation, not only linking. Skip only the redundant sanity check when a
# specs file is present; ESP-IDF has already validated the configured compiler.
set(THORVG_SKIP_MESON_SANITY_CHECK false)
foreach(flag IN LISTS compiler_args)
    if(flag MATCHES "^-specs=")
        set(THORVG_SKIP_MESON_SANITY_CHECK true)
        break()
    endif()
endforeach()

# ThorVG's src/meson.build runs find_library('pthread') unconditionally on
# every non-Windows/Android/Emscripten system. The probe links -lpthread with
# the component's cpp_link_args on the command line, so -L decides what it
# finds: point it at the real IDF pthread, and add_dependencies() below
# guarantees pthread is built before Meson probes for it.
idf_component_get_property(pthread_lib pthread COMPONENT_LIB GENERATOR_EXPRESSION)
list(APPEND linker_args "-L$<TARGET_FILE_DIR:${pthread_lib}>")

# ---------------------------------------------------------------------------
# Cross file and Meson invocations
# ---------------------------------------------------------------------------
thorvg_join_meson_array(MESON_CXX_FLAGS ${compiler_args})
thorvg_join_meson_array(MESON_LINKER_FLAGS ${linker_args})

# Everything that can change between reconfigurations.
# Fixed options (prefix, libdir, threads, ...) stay in
# CONFIGURE_COMMAND below: they never differ between builds
set(THORVG_MESON_VOLATILE_ARGS
    "-Dlog=${THORVG_LOG}"
    "-Dloaders=${THORVG_LOADERS_OPTION}"
    "-Dcpp_args=${MESON_CXX_FLAGS}"
    "-Dcpp_link_args=${MESON_LINKER_FLAGS}")

# Meson has no Xtensa cpu family. ThorVG uses this value only to enable x86/ARM
# SIMD paths, neither of which applies to ESP targets, so "riscv32" safely
# keeps SIMD disabled for both Xtensa and RISC-V ESP chips.
set(cpu_family "riscv32")

configure_file("${CMAKE_CURRENT_LIST_DIR}/cross_file.txt.in" "${THORVG_CROSS_CFG}")

include(ExternalProject)
ExternalProject_Add(${THORVG_LIB}_target
    PREFIX "${THORVG_EP_PREFIX}"
    SOURCE_DIR "${THORVG_SUBDIR}"
    BINARY_DIR "${THORVG_BUILD_DIR}"
    CONFIGURE_COMMAND "${MESON_EXECUTABLE}" setup "${THORVG_BUILD_DIR}" "${THORVG_SUBDIR}"
        --cross-file "${THORVG_CROSS_CFG}"
        -Dbindings=capi
        -Dengines=cpu
        -Dextra=
        -Ddefault_library=static # build static library
        -Db_staticpic=false # no -fPIC
        -Doptimization=plain # Upstream default_options use -O3. Leave optimization to IDF's -O*.
        -Dprefix=${THORVG_INSTALL_DIR} # install prefix; fixed per build directory
        -Dlibdir=lib
        -Dthreads=true # IDF is a multi-threaded RTOS environment
        ${THORVG_MESON_VOLATILE_ARGS}
    BUILD_COMMAND "${MESON_EXECUTABLE}" compile -C "${THORVG_BUILD_DIR}"
    INSTALL_COMMAND "${MESON_EXECUTABLE}" install -C "${THORVG_BUILD_DIR}" --no-rebuild
    BUILD_BYPRODUCTS "${THORVG_STATIC_LIB}"
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
# The cross file is generated at CMake configure time; register it at file
# level (not as a custom target) so a content change invalidates the stamp of
# the configure step.
ExternalProject_Add_StepDependencies(${THORVG_LIB}_target configure "${THORVG_CROSS_CFG}")

add_prebuilt_library(${THORVG_LIB} "${THORVG_STATIC_LIB}" PRIV_REQUIRES pthread)
target_include_directories(${THORVG_LIB} INTERFACE "${THORVG_INC_DIR}")
add_dependencies(${THORVG_LIB} ${THORVG_LIB}_target)
add_dependencies(${THORVG_LIB}_target idf::pthread)

target_link_libraries(${COMPONENT_LIB} INTERFACE ${THORVG_LIB})
