
set(co_srcs
    "src/CO_driver_esp32.c"
    "CANopenNode/CANopen.c"
    "CANopenNode/301/CO_NMT_Heartbeat.c"
    "CANopenNode/301/CO_Emergency.c"
    "CANopenNode/301/CO_ODinterface.c"
)

idf_component_register(
    SRCS ${co_srcs}
    INCLUDE_DIRS "CANopenNode" "src"
    REQUIRES esp_driver_twai
)
