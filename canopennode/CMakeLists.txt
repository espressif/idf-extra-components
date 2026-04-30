
set(co_srcs
    "src/CO_driver_esp32.c"
    "CANopenNode/CANopen.c"
    "CANopenNode/301/CO_NMT_Heartbeat.c"
    "CANopenNode/301/CO_Emergency.c"
    "CANopenNode/301/CO_ODinterface.c"
)

if(CONFIG_CO_SDO_SERVER)
    list(APPEND co_srcs "CANopenNode/301/CO_SDOserver.c")
endif()

if(CONFIG_CO_SDO_CLIENT)
    list(APPEND co_srcs
        "CANopenNode/301/CO_SDOclient.c"
        "CANopenNode/301/CO_fifo.c"
    )
endif()

if(CONFIG_CO_PDO)
    list(APPEND co_srcs "CANopenNode/301/CO_PDO.c")
endif()

idf_component_register(
    SRCS ${co_srcs}
    INCLUDE_DIRS "CANopenNode" "src"
    REQUIRES esp_driver_twai
)
