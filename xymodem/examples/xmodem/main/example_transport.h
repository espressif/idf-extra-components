/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "xymodem.h"

esp_err_t example_transport_init(xymodem_transport_t *transport);
void example_transport_deinit(void);
