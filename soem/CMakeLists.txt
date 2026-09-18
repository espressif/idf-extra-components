set(SOEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/soem")

# SOEM build-time resource limits
set(EC_BUFSIZE EC_MAXECATFRAME)
set(EC_MAXBUF ${CONFIG_EC_MAXBUF})
set(EC_MAXEEPBITMAP ${CONFIG_EC_MAXEEPBITMAP})
set(EC_MAXEEPBUF "EC_MAXEEPBITMAP << 5")
set(EC_LOGGROUPOFFSET ${CONFIG_EC_LOGGROUPOFFSET})
set(EC_MAXELIST ${CONFIG_EC_MAXELIST})
set(EC_MAXNAME ${CONFIG_EC_MAXNAME})
set(EC_MAXSLAVE ${CONFIG_EC_MAXSLAVE})
set(EC_MAXGROUP ${CONFIG_EC_MAXGROUP})
set(EC_MAXIOSEGMENTS ${CONFIG_EC_MAXIOSEGMENTS})
set(EC_MAXMBX ${CONFIG_EC_MAXMBX})
set(EC_MBXPOOLSIZE ${CONFIG_EC_MBXPOOLSIZE})
set(EC_MAXEEPDO ${CONFIG_EC_MAXEEPDO})
set(EC_MAXSM ${CONFIG_EC_MAXSM})
set(EC_MAXFMMU ${CONFIG_EC_MAXFMMU})
set(EC_MAXLEN_ADAPTERNAME ${CONFIG_EC_MAXLEN_ADAPTERNAME})
set(EC_MAX_MAPT ${CONFIG_EC_MAX_MAPT})
set(EC_MAXODLIST ${CONFIG_EC_MAXODLIST})
set(EC_MAXOELIST ${CONFIG_EC_MAXOELIST})
set(EC_SOE_MAXNAME ${CONFIG_EC_SOE_MAXNAME})
set(EC_SOE_MAXMAPPING ${CONFIG_EC_SOE_MAXMAPPING})

# SOEM timeouts are expressed in microseconds
set(EC_TIMEOUTRET ${CONFIG_EC_TIMEOUTRET})
set(EC_TIMEOUTRET3 "EC_TIMEOUTRET * 3")
set(EC_TIMEOUTSAFE ${CONFIG_EC_TIMEOUTSAFE})
set(EC_TIMEOUTEEP ${CONFIG_EC_TIMEOUTEEP})
set(EC_TIMEOUTTXM ${CONFIG_EC_TIMEOUTTXM})
set(EC_TIMEOUTRXM ${CONFIG_EC_TIMEOUTRXM})
set(EC_TIMEOUTSTATE ${CONFIG_EC_TIMEOUTSTATE})
set(EC_DEFAULTRETRIES ${CONFIG_EC_DEFAULTRETRIES})

# Logical MAC patterns used by SOEM for path identification
set(EC_PRIMARY_MAC_ARRAY "{0x0101, 0x0101, 0x0101}")
set(EC_SECONDARY_MAC_ARRAY "{0x0404, 0x0404, 0x0404}")

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    configure_file(
        "${SOEM_ROOT}/include/soem/ec_options.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/include/soem/ec_options.h"
    )
endif()

set(SOEM_CORE_SOURCES
    "${SOEM_ROOT}/src/ec_base.c"
    "${SOEM_ROOT}/src/ec_config.c"
    "${SOEM_ROOT}/src/ec_main.c"
    "${SOEM_ROOT}/src/ec_print.c"
    "${SOEM_ROOT}/src/ec_dc.c"
    "${SOEM_ROOT}/src/ec_coe.c"
    "${SOEM_ROOT}/src/ec_eoe.c"
    "${SOEM_ROOT}/src/ec_foe.c"
    "${SOEM_ROOT}/src/ec_soe.c"
)

set(SOEM_PORT_SOURCES
    "port/esp_soem.c"
    "port/osal/osal.c"
    "port/oshw/oshw.c"
    "port/oshw/nicdrv.c"
)

idf_component_register(
    SRCS
        ${SOEM_CORE_SOURCES}
        ${SOEM_PORT_SOURCES}

    # Prefer ESP-IDF port headers over upstream platform headers.
    INCLUDE_DIRS
        "port/include"
        "port/oshw"
        "port/osal"
        "${SOEM_ROOT}/osal"
        "${SOEM_ROOT}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/include"

    REQUIRES
        esp_eth

    PRIV_REQUIRES
        esp_timer
)
