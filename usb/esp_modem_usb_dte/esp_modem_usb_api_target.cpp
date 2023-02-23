/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_terminal.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "exception_stub.hpp"
#include "esp_modem_usb_config.h"
#include "esp_modem_config.h"

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
static const char *TAG = "modem_usb_api_target";
#endif

namespace esp_modem {
std::shared_ptr<DTE> create_usb_dte(const dte_config *config)
{
    if (config == nullptr) {
        return nullptr;
    }

    // *INDENT-OFF*
    TRY_CATCH_RET_NULL(
        auto primary_term = create_usb_terminal(config);
        auto *usb_config = static_cast<struct esp_modem_usb_term_config *>(config->extension_config);
        if (usb_config->secondary_interface_idx > -1) {
            auto secondary_term = create_usb_terminal(config, 1);
            return std::make_shared<DTE>(config, std::move(primary_term), std::move(secondary_term));
        }
        return std::make_shared<DTE>(config, std::move(primary_term));
    )
    // *INDENT-ON*
}
}
