idf_component_register(
    SRCS "blemesh_example_main.c"
    INCLUDE_DIRS "."
    REQUIRES blemesh_manager bt console nvs_flash driver
)
