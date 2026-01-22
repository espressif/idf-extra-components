idf_component_register(
    SRCS "completion.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES esp_linenoise
)
