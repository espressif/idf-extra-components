# Enable Swift support early, before component registration
# Force Whole Module builds (required by Embedded Swift)
# Use CMAKE_Swift_COMPILER_WORKS to skip trial compilations which don't work when cross-compiling
set(CMAKE_Swift_COMPILER_WORKS YES)
set(CMAKE_Swift_COMPILATION_MODE_DEFAULT wholemodule)
set(CMAKE_Swift_COMPILATION_MODE wholemodule)
enable_language(Swift)

function(force_cxx_linker)
    # Get the executable target name from ESP-IDF build properties
    idf_build_get_property(project_elf EXECUTABLE)

    if(TARGET ${project_elf})
        set_target_properties(${project_elf} PROPERTIES LINKER_LANGUAGE CXX)
        message(STATUS "Swift: Forced ${project_elf} to use CXX linker")
    else()
        message(FATAL_ERROR "Swift: Cannot set linker language - executable target not found")
    endif()
endfunction()

# Automatically defer the call to force CXX linker after all targets are created
# This ensures the final executable uses CXX linker instead of swiftc
cmake_language(DEFER CALL force_cxx_linker)

# ------------------------------------------------------------------------------
# Post-process __idf_build_target compile definitions for Swift compatibility
# ------------------------------------------------------------------------------
# Definitions set via idf_build_set_property(COMPILE_DEFINITIONS ...) (e.g.
# IDF_VER="...") may be applied directly to __idf_build_target's
# COMPILE_DEFINITIONS without going through target_compile_definitions().
#
# This deferred function runs after all components are configured and rewrites
# any remaining MACRO=VALUE items on __idf_build_target with COMPILE_LANGUAGE
# guard expressions, using the same logic as the target_compile_definitions()
# override below.
#
# See also "target_compile_definitions" below
# ------------------------------------------------------------------------------
function(swift_fixup_build_definitions)
    if(NOT TARGET __idf_build_target)
        return()
    endif()

    idf_build_get_property(_defs COMPILE_DEFINITIONS)
    if(NOT _defs)
        return()
    endif()

    set(_new_defs "")
    set(_modified FALSE)
    foreach(_def IN LISTS _defs)
        if(_def MATCHES "COMPILE_LANGUAGE" OR NOT _def MATCHES "=")
            list(APPEND _new_defs "${_def}")
        else()
            list(APPEND _new_defs "$<$<COMPILE_LANGUAGE:C,CXX,ASM>:${_def}>")
            set(_modified TRUE)
        endif()
    endforeach()

    if(_modified)
        idf_build_set_property(COMPILE_DEFINITIONS "${_new_defs}")
    endif()
endfunction()

cmake_language(DEFER CALL swift_fixup_build_definitions)

# ------------------------------------------------------------------------------
# Override target_compile_definitions() for Swift compatibility
# ------------------------------------------------------------------------------
# Wraps the built-in CMake command:
#
#   target_compile_definitions(<target>
#       <INTERFACE|PUBLIC|PRIVATE> [items1...]
#       [<INTERFACE|PUBLIC|PRIVATE> [items2...] ...])
#
# The Embedded Swift compiler only supports -DMACRO, not -DMACRO=VALUE. The
# Swift driver rejects the =VALUE form with a warning at command-line parsing
# time (before the frontend), so -suppress-warnings cannot silence it.
#
# For every MACRO=VALUE item, this macro wraps it in a language-conditional
# generator expression so only C/CXX/ASM sees it:
#   $<$<COMPILE_LANGUAGE:C,CXX,ASM>:MACRO=VALUE>
#
# The definition is dropped entirely for Swift -- passing just -DMACRO
# (without =VALUE) could silently change program semantics.
#
# Items that already contain a COMPILE_LANGUAGE generator expression, or that
# have no '=' (plain -DMACRO flags), are passed through unchanged.
#
# Visibility keywords (PUBLIC, PRIVATE, INTERFACE) and their grouping are
# preserved. C/C++/ASM compilation is unaffected -- it still sees the full
# MACRO=VALUE.
#
# TODO: generate MACRO=VALUE data into a bridging header so Swift code can
# access these definitions.
#
# Because project_include.cmake runs at project scope before any component
# CMakeLists.txt, the override intercepts definitions from all ESP-IDF
# components automatically.
# ------------------------------------------------------------------------------
macro(target_compile_definitions _swift_target)
    set(_all_args ${ARGN})
    set(_out_args "")
    foreach(_arg IN LISTS _all_args)
        if(_arg STREQUAL "PUBLIC" OR
           _arg STREQUAL "PRIVATE" OR
           _arg STREQUAL "INTERFACE" OR
           _arg MATCHES "COMPILE_LANGUAGE" OR
           NOT _arg MATCHES "=")
            list(APPEND _out_args "${_arg}")
        else()
            list(APPEND _out_args
                "$<$<COMPILE_LANGUAGE:C,CXX,ASM>:${_arg}>")
                # Intentionally omitted for Swift: passing just -DMACRO
                # (without =VALUE) could silently change program semantics,
                # so the definition is dropped entirely for the Swift target.
                #
                # TODO: generate data to briging header to provide these macros
        endif()
    endforeach()
    _target_compile_definitions(${_swift_target} ${_out_args})
endmacro()
