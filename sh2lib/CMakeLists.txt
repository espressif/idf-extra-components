idf_component_register(SRCS "sh2lib.c"
                    INCLUDE_DIRS .
                    REQUIRES http_parser
                    PRIV_REQUIRES lwip esp-tls)
