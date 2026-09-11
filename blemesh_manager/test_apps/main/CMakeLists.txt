idf_component_register(SRCS "blemesh_manager_test.c"
                    INCLUDE_DIRS "."
                    REQUIRES blemesh_manager bt
                    PRIV_REQUIRES unity)
