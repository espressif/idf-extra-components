# Swift Support Component for ESP-IDF

idf_build_get_property(arch IDF_TARGET_ARCH)
if("${arch}" STREQUAL "xtensa")
    idf_build_get_property(target IDF_TARGET)
    message(FATAL_ERROR "Embedded Swift not supported on Xtensa architecture (target: ${target})")
endif()

# Register this as a header-only component with no C/C++ sources.
# Note: Include directories from the C/C++ toolchain sysroot are added to provide
#       necessary system headers that Swift code may reference via the bridging header.
idf_component_register(
    INCLUDE_DIRS ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES} ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}
)

# ------------------------------------------------------------------------------
# extract_riscv_flags()
# ------------------------------------------------------------------------------
# Extracts RISC-V architecture flags (-march and -mabi) from the ESP-IDF toolchain
# configuration and formats them for use with the Swift compiler.
#
# The Swift compiler needs to know the target architecture flags to generate
# compatible code. This function extracts these flags from ESP-IDF's C toolchain
# configuration, applies necessary transformations, and formats them as Swift
# compiler arguments.
#
# Arguments:
#   out_arch_flags - Output variable name to store the formatted architecture flags
#
# Returns (via PARENT_SCOPE):
#   String containing Swift-compatible architecture flags in the format:
#   "-Xcc -march=<arch> -Xcc -mabi=<abi>"
# ------------------------------------------------------------------------------
function(extract_riscv_flags out_arch_flags)
    set(march_flag "")
    set(mabi_flag "")

    # Method 1: Extract flags from ESP-IDF's cflags response file (ESP-IDF 6.0+)
    if(DEFINED IDF_TOOLCHAIN_BUILD_DIR AND EXISTS "${IDF_TOOLCHAIN_BUILD_DIR}/cflags")
        file(STRINGS "${IDF_TOOLCHAIN_BUILD_DIR}/cflags" cflags_lines)
        foreach(line IN LISTS cflags_lines)
            if(line MATCHES "^-march=")
                set(march_flag "${line}")
            elseif(line MATCHES "^-mabi=")
                set(mabi_flag "${line}")
            endif()
        endforeach()
    endif()

    # Method 2: Fallback to parsing CMAKE_C_FLAGS directly (ESP-IDF 5.x and earlier)
    if(NOT march_flag OR NOT mabi_flag)
        string(REGEX MATCH "-march=[^ ]+" march_flag_fallback "${CMAKE_C_FLAGS}")
        string(REGEX MATCH "-mabi=[^ ]+" mabi_flag_fallback "${CMAKE_C_FLAGS}")
        if(NOT march_flag AND march_flag_fallback)
            set(march_flag "${march_flag_fallback}")
        endif()
        if(NOT mabi_flag AND mabi_flag_fallback)
            set(mabi_flag "${mabi_flag_fallback}")
        endif()
    endif()

    # Apply default ABI if not found in toolchain configuration.
    if(NOT mabi_flag)
        set(mabi_flag "-mabi=ilp32")
    endif()

    # Remove Espressif-specific ISA extensions that are not supported by Swift.
    # Espressif adds custom extensions (prefixed with "_x") to the -march flag
    # that the Swift compiler does not recognize. These must be stripped out.
    if(march_flag)
        string(REGEX REPLACE "_x[^ ]*" "" march_flag "${march_flag}")
    endif()

    message(STATUS "Swift: Using arch flags '${march_flag}' '${mabi_flag}'")

    # Format the flags for the Swift compiler using -Xcc to pass them to Clang.
    # The Swift compiler uses Clang for C interoperability, so architecture flags
    # must be prefixed with -Xcc to be properly forwarded.
    set(${out_arch_flags} "-Xcc ${march_flag} -Xcc ${mabi_flag}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# idf_component_register_swift()
# ------------------------------------------------------------------------------
# Configures a CMake target for Embedded Swift compilation within the ESP-IDF
# build system. This function applies the necessary compiler flags and settings
# to compile Swift source files for embedded targets.
#
# This function should be called after idf_component_register() to configure
# Swift compilation for components that contain Swift source code.
#
# Arguments:
#   target_component - The CMake library target (typically ${COMPONENT_LIB})
#
# Named Arguments:
#   BRIDGING_HEADER - Path to the C bridging header file that exposes C APIs
#                     to Swift code. Required.
#   SRCS            - List of Swift source files to compile. Required.
# ------------------------------------------------------------------------------
function(idf_component_register_swift target_component)
    # Parse the function's named arguments (BRIDGING_HEADER and SRCS).
    cmake_parse_arguments(SWIFT "" "BRIDGING_HEADER" "SRCS" ${ARGN})

    # Validate required parameters.
    if(NOT SWIFT_BRIDGING_HEADER)
        message(FATAL_ERROR "idf_component_register_swift: BRIDGING_HEADER is required")
    endif()
    if(NOT SWIFT_SRCS)
        message(FATAL_ERROR "idf_component_register_swift: SRCS is required")
    endif()

    # Extract RISC-V architecture flags from the ESP-IDF toolchain configuration.
    extract_riscv_flags(arch_flags)

    # Clear default compile options inherited from the C/C++ toolchain.
    # The Swift compiler does not accept C/C++-specific flags, so we must
    # start with a clean state and add only Swift-compatible options.
    set_target_properties(${target_component} PROPERTIES COMPILE_OPTIONS "")

    # Configure Swift compiler flags for Embedded Swift.
    target_compile_options(${target_component} PUBLIC "$<$<COMPILE_LANGUAGE:Swift>:SHELL:
        -target riscv32-none-none-eabi
        ${arch_flags}
        -Xfrontend -function-sections
        -enable-experimental-feature Embedded
        -wmo
        -parse-as-library
        -Osize
        -Xcc -fno-pic
        -Xcc -fno-pie
        -pch-output-dir ${CMAKE_CURRENT_BINARY_DIR}/pch
        -Xfrontend -enable-single-module-llvm-emission
        -import-bridging-header ${SWIFT_BRIDGING_HEADER}
    >")

    add_custom_command(
        TARGET ${target_component}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --remove-section .swift_modhash
                $<TARGET_FILE:${target_component}> $<TARGET_FILE:${target_component}>
        COMMENT "Removing .swift_modhash section from ${target_component}"
    )

    # Register Swift source files with the target.
    # These files will be compiled with the Swift compiler using the flags
    # configured above.
    target_sources(${target_component}
        PRIVATE
        ${SWIFT_SRCS}
    )
endfunction()
