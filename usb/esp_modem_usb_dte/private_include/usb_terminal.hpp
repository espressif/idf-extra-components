/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "cxx_include/esp_modem_dte.hpp"
#include "../private_include/exception_stub.hpp"

struct esp_modem_dte_config;

namespace esp_modem {
std::unique_ptr<Terminal> create_usb_terminal(const esp_modem_dte_config *config);
}  // namespace esp_modem
