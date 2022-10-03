/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_terminal.hpp"
#include "cxx_include/esp_modem_api.hpp"

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
static const char *TAG = "modem_usb_api_target";
#endif

namespace esp_modem {
std::shared_ptr<DTE> create_usb_dte(const dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = create_usb_terminal(config);
        return std::make_shared<DTE>(config, std::move(term));
    )
}
}
