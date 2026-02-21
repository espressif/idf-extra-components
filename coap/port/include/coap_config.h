/*
 * libcoap configure implementation for ESP32 platform.
 *
 * coap.h -- main header file for CoAP stack of libcoap
 *
 * Copyright (C) 2010-2012,2015-2025 Olaf Bergmann <bergmann@tzi.org>
 *               2015 Carsten Schoenert <c.schoenert@t-online.de>
 *
 * Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_CONFIG_H_
#define COAP_CONFIG_H_

/* Always enabled in ESP-IDF */
#ifndef WITH_POSIX
#define WITH_POSIX
#endif

#include "coap_config_posix.h"

#define HAVE_STDIO_H
#define HAVE_ASSERT_H
#define HAVE_INTTYPES_H

#define PACKAGE_STRING PACKAGE_NAME PACKAGE_VERSION

/* it's just provided by libc. i hope we don't get too many of those, as
 * actually we'd need autotools again to find out what environment we're
 * building in */
#define HAVE_STRNLEN 1

#define HAVE_LIMITS_H

#define COAP_RESOURCES_NOHASH

/* Note: If neither of COAP_CLIENT_SUPPORT or COAP_SERVER_SUPPORT is set,
   then libcoap sets both for backward compatibility */
#ifdef CONFIG_COAP_CLIENT_SUPPORT
#define COAP_CLIENT_SUPPORT 1
#endif /* CONFIG_COAP_CLIENT_SUPPORT */

#ifdef CONFIG_COAP_SERVER_SUPPORT
#define COAP_SERVER_SUPPORT 1
#endif /* CONFIG_COAP_SERVER_SUPPORT */

#ifdef CONFIG_COAP_PROXY_SUPPORT
#define COAP_PROXY_SUPPORT 1
#endif /* CONFIG_COAP_PROXY_SUPPORT */

#ifdef CONFIG_LWIP_IPV4
#define COAP_IPV4_SUPPORT 1
#else /* ! CONFIG_LWIP_IPV4 */
struct sockaddr_in {
    u8_t            sin_len;
    sa_family_t     sin_family;
    in_port_t       sin_port;
    struct in_addr  sin_addr;
};
#endif /* ! CONFIG_LWIP_IPV4 */

#ifdef CONFIG_LWIP_IPV6
#define COAP_IPV6_SUPPORT 1
#else /* ! CONFIG_LWIP_IPV6 */
struct sockaddr_in6 {
    u8_t            sin6_len;
    sa_family_t     sin6_family;
    in_port_t       sin6_port;
    u32_t           sin6_flowinfo;
    struct in_addr  sin6_addr;
    u32_t           sin6_scope_id;
};
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 40
#endif /* INET6_ADDRSTRLEN */
#endif /* ! CONFIG_LWIP_IPV6 */

#ifdef CONFIG_COAP_ASYNC_SUPPORT
#define COAP_ASYNC_SUPPORT 1
#endif /* CONFIG_COAP_ASYNC_SUPPORT */

#ifdef CONFIG_COAP_TCP_SUPPORT
#define COAP_DISABLE_TCP 0
#else /* ! CONFIG_COAP_TCP_SUPPORT */
#define COAP_DISABLE_TCP 1
#endif /* ! CONFIG_COAP_TCP_SUPPORT */

#ifdef CONFIG_COAP_OSCORE_SUPPORT
#define COAP_OSCORE_SUPPORT 1
#else /* ! CONFIG_COAP_OSCORE_SUPPORT */
#define COAP_OSCORE_SUPPORT 0
#endif /* ! CONFIG_COAP_OSCORE_SUPPORT */

#ifdef CONFIG_COAP_WEBSOCKETS
#define COAP_WS_SUPPORT 1
#else /* ! CONFIG_COAP_WEBSOCKETS */
#define COAP_WS_SUPPORT 0
#endif /* ! CONFIG_COAP_WEBSOCKETS */

#ifdef CONFIG_COAP_OBSERVE_PERSIST
#define COAP_WITH_OBSERVE_PERSIST 1
#else /* ! CONFIG_COAP_OBSERVE_PERSIST */
#define COAP_WITH_OBSERVE_PERSIST 0
#endif /* ! CONFIG_COAP_OBSERVE_PERSIST */

#ifdef CONFIG_COAP_Q_BLOCK
#define COAP_Q_BLOCK_SUPPORT 1
#else /* ! CONFIG_COAP_Q_BLOCK */
#define COAP_Q_BLOCK_SUPPORT 0
#endif /* ! CONFIG_COAP_Q_BLOCK */

#ifdef CONFIG_COAP_THREAD_RECURSIVE_CHECK
#define COAP_THREAD_RECURSIVE_CHECK 1
#else /* ! CONFIG_COAP_THREAD_RECURSIVE_CHECK */
#define COAP_THREAD_RECURSIVE_CHECK 0
#endif /* ! CONFIG_COAP_THREAD_RECURSIVE_CHECK */

#ifdef CONFIG_COAP_THREAD_SAFE
#define COAP_THREAD_SAFE 1
#else /* ! CONFIG_COAP_THREAD_SAFE */
#define COAP_THREAD_SAFE 0
#endif /* ! CONFIG_COAP_THREAD_SAFE */

#ifdef CONFIG_COAP_DEBUGGING
#define COAP_MAX_LOGGING_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL
#else /* ! CONFIG_COAP_DEBUGGING */
#define COAP_MAX_LOGGING_LEVEL 0
#endif /* ! CONFIG_COAP_DEBUGGING */

#endif /* COAP_CONFIG_H_ */
