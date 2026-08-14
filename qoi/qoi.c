/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * This is the single translation unit that instantiates the QOI library
 * implementation. QOI is a single-header, single-implementation library:
 * defining QOI_IMPLEMENTATION before including qoi.h makes this file the only
 * place where the qoi_encode()/qoi_decode()/qoi_write()/qoi_read() functions
 * are compiled. All other users of the component just #include "qoi.h" and
 * link against this compiled object.
 */
#define QOI_IMPLEMENTATION
#include "qoi.h"
