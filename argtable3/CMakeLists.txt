# This creates a virtual include file with an "argtable3" subdirectory where
# "/argtable3/src/argtable3.h" is copied. the virtual directory is then used as
# the include folder. This is so we can keep including argtable3 header as follow:
# #include "argtable3/argtable3.h"
set(VIRTUAL_INCLUDE_DIR "${CMAKE_BINARY_DIR}/virtual_include")
file(MAKE_DIRECTORY "${VIRTUAL_INCLUDE_DIR}/argtable3")
file(COPY "${COMPONENT_PATH}/argtable3/src/argtable3.h"
     DESTINATION "${VIRTUAL_INCLUDE_DIR}/argtable3")

idf_component_register(SRCS "argtable3/src/arg_cmd.c"
                            "argtable3/src/arg_date.c"
                            "argtable3/src/arg_dbl.c"
                            "argtable3/src/arg_dstr.c"
                            "argtable3/src/arg_end.c"
                            "argtable3/src/arg_file.c"
                            "argtable3/src/arg_hashtable.c"
                            "argtable3/src/arg_int.c"
                            "argtable3/src/arg_lit.c"
                            "argtable3/src/arg_rem.c"
                            "argtable3/src/arg_rex.c"
                            "argtable3/src/arg_str.c"
                            "argtable3/src/arg_utils.c"
                            "argtable3/src/argtable3.c"
                    INCLUDE_DIRS "${VIRTUAL_INCLUDE_DIR}")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
        ARG_ENABLE_LOG=0
        ARG_REPLACE_GETOPT=0
)
