set(include_dirs port/include port/include libcoap/include)

set(srcs
    "libcoap/src/coap_address.c"
    "libcoap/src/coap_asn1.c"
    "libcoap/src/coap_async.c"
    "libcoap/src/coap_block.c"
    "libcoap/src/coap_cache.c"
    "libcoap/src/coap_debug.c"
    "libcoap/src/coap_dtls.c"
    "libcoap/src/coap_encode.c"
    "libcoap/src/coap_event.c"
    "libcoap/src/coap_hashkey.c"
    "libcoap/src/coap_io.c"
    "libcoap/src/coap_layers.c"
    "libcoap/src/coap_mbedtls.c"
    "libcoap/src/coap_mem.c"
    "libcoap/src/coap_net.c"
    "libcoap/src/coap_netif.c"
    "libcoap/src/coap_notls.c"
    "libcoap/src/coap_option.c"
    "libcoap/src/coap_oscore.c"
    "libcoap/src/coap_pdu.c"
    "libcoap/src/coap_prng.c"
    "libcoap/src/coap_resource.c"
    "libcoap/src/coap_session.c"
    "libcoap/src/coap_str.c"
    "libcoap/src/coap_subscribe.c"
    "libcoap/src/coap_tcp.c"
    "libcoap/src/coap_time.c"
    "libcoap/src/coap_uri.c"
    "libcoap/src/coap_ws.c")

if(CONFIG_COAP_OSCORE_SUPPORT)
    list(APPEND srcs
        "libcoap/src/oscore/oscore.c"
        "libcoap/src/oscore/oscore_cbor.c"
        "libcoap/src/oscore/oscore_context.c"
        "libcoap/src/oscore/oscore_cose.c"
        "libcoap/src/oscore/oscore_crypto.c")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    REQUIRES lwip mbedtls)

