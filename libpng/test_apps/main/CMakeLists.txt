idf_component_register(
    SRCS test_libpng.c test_main.c
    PRIV_REQUIRES unity
    WHOLE_ARCHIVE
    EMBED_FILES "in.png" "out.pgm")
