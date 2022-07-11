idf_component_register(SRCS "sh2lib.c"
                    INCLUDE_DIRS .
                    REQUIRES http_parser
                    PRIV_REQUIRES lwip esp-tls)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
