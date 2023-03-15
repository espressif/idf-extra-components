/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "cxx_include/esp_modem_api.hpp"

namespace esp_modem {
/**
 * @brief Create USB DTE
 *
 * @param[in] config DTE configuration
 * @return shared ptr to DTE on success
 *         nullptr on failure (either due to insufficient memory or wrong dte configuration)
 *         if exceptions are disabled the API abort()'s on error
 */
std::shared_ptr<DTE> create_usb_dte(const dte_config *config);
}
