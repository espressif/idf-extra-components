idf_component_register(INCLUDE_DIRS . libpng
                       SRC_DIRS libpng)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-maybe-uninitialized)
