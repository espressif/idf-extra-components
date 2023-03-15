/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "cxx_include/esp_modem_dte.hpp"

struct esp_modem_dte_config;

namespace esp_modem {
/**
 * @brief Create a usb terminal object
 *
 * @param[in] config    DTE USB configuration
 * @param[in] term_idx  Terminal index. 0: primary terminal, 1: secondary terminal.
 * @return std::unique_ptr<Terminal>
 */
std::unique_ptr<Terminal> create_usb_terminal(const esp_modem_dte_config *config, int term_idx = 0);
}  // namespace esp_modem
