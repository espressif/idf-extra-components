idf_component_register(SRCS "tinycbor/src/cborencoder_close_container_checked.c"
                            "tinycbor/src/cborencoder.c"
                            "tinycbor/src/cborencoder_float.c"
                            "tinycbor/src/cborerrorstrings.c"
                            "tinycbor/src/cborparser_dup_string.c"
                            "tinycbor/src/cborparser.c"
                            "tinycbor/src/cborparser_float.c"
                            "tinycbor/src/cborpretty_stdio.c"
                            "tinycbor/src/cborpretty.c"
                            "tinycbor/src/cbortojson.c"
                            "tinycbor/src/cborvalidation.c"
                            "tinycbor/src/open_memstream.c"
                    INCLUDE_DIRS "port/include"
                    PRIV_INCLUDE_DIRS "tinycbor/src")

# for open_memstream.c
set_source_files_properties(tinycbor/src/open_memstream.c PROPERTIES
                            COMPILE_DEFINITIONS "__linux__")

# Fix unreachable macro redefinition issue between ESP-IDF toolchain and tinycbor
# by force-including a header that resolves the conflict
target_compile_options(${COMPONENT_LIB} PRIVATE
                       "-include" "${CMAKE_CURRENT_SOURCE_DIR}/port/include/unreachable_fix.h")
