set(src "test_main.c" "test_iqmath.c")

idf_component_register(SRCS ${src}
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity
                       WHOLE_ARCHIVE)
