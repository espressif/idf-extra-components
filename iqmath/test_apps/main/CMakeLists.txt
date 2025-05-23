# This is the project CMakeLists.txt file for the test subproject
set(src "test_app_main.c" "test_iqmath.c")

set(priv_reqs unity)

idf_component_register(SRCS ${src}
                       PRIV_REQUIRES ${priv_reqs}
                       WHOLE_ARCHIVE
                       )
