file(GLOB CANOPENNODE_CORE_SRCS
    "CANopenNode/301/*.c"
    "CANopenNode/303/*.c"
    "CANopenNode/304/*.c"
    "CANopenNode/305/*.c"
    "CANopenNode/309/*.c"
    "CANopenNode/extra/*.c"
)

idf_component_register(
    SRCS
        "src/CO_driver_esp32.c"
        "CANopenNode/CANopen.c"
        ${CANOPENNODE_CORE_SRCS}
    INCLUDE_DIRS "CANopenNode" "src"
    REQUIRES esp_driver_twai
)

# Build CANopen core in multi-OD mode, so this component doesn't depend on an
# application-local OD.h at component compile time.
target_compile_definitions(${COMPONENT_LIB} PUBLIC CO_MULTIPLE_OD)
