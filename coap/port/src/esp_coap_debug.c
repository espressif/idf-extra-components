/* coap_debug.c -- debug utilities
 *
 * Copyright (C) 2010--2012,2014--2022 Olaf Bergmann <bergmann@tzi.org> and others
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use.
 */

/**
 * @file coap_debug.c
 * @brief Debug utilities
 */

#include "coap3/coap_internal.h"

coap_log_t coap_get_log_level(void)
{
    return LOG_WARNING;
}

void coap_set_log_level(coap_log_t level)
{

}


void coap_set_log_handler(coap_log_handler_t handler)
{

}

const char *coap_package_name(void)
{
    return PACKAGE_NAME;
}

const char *coap_package_version(void)
{
    return PACKAGE_STRING;
}

const char *coap_package_build(void)
{
#ifdef LIBCOAP_PACKAGE_BUILD
    return LIBCOAP_PACKAGE_BUILD;
#else /* !LIBCOAP_PACKAGE_BUILD */
    return PACKAGE_STRING;
#endif /* !LIBCOAP_PACKAGE_BUILD */
}

void coap_set_show_pdu_output(int use_fprintf)
{

}

void coap_show_pdu(coap_log_t level, const coap_pdu_t *pdu)
{

}

void coap_show_tls_version(coap_log_t level)
{

}

char *coap_string_tls_version(char *buffer, size_t bufsize)
{
    return buffer;
}

void coap_log_impl(coap_log_t level, const char *format, ...)
{

}

char *coap_string_tls_support(char *buffer, size_t bufsize)
{
    return buffer;
}

size_t coap_print_addr(const coap_address_t *addr,
                       unsigned char *buf, size_t len)
{
    buf[0] = '\000';
    return 0;
}


int coap_debug_set_packet_loss(const char *loss_level)
{
    return 0;
}

int coap_debug_send_packet(void)
{
    return 1;
}
