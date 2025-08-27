/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef CBOR_UNREACHABLE_FIX_H
#define CBOR_UNREACHABLE_FIX_H

/* This header fixes the unreachable macro redefinition issue between
   ESP-IDF toolchain and tinycbor library */

/* Force include stddef.h first to get the ESP-IDF definition */
#include <stddef.h>

/* Now undefine it so tinycbor can define its own version */
#ifdef unreachable
#undef unreachable
#endif

#endif /* CBOR_UNREACHABLE_FIX_H */
