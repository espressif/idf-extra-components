idf_component_register(SRCS quirc/lib/decode.c
                            quirc/lib/identify.c
                            quirc/lib/quirc.c
                            quirc/lib/version_db.c
                       INCLUDE_DIRS quirc/lib)

# Performance optimization; see quirc README.md for an explanation of these options
target_compile_definitions(${COMPONENT_LIB} PRIVATE QUIRC_FLOAT_TYPE=float)
target_compile_definitions(${COMPONENT_LIB} PRIVATE QUIRC_USE_TGMATH)
