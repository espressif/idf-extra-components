/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_modem_usb_c_api.h"
#include "cxx_include/esp_modem_usb_api.hpp"
#include "esp_private/c_api_wrapper.hpp"

using namespace esp_modem;

extern "C" esp_modem_dce_t *esp_modem_new_dev_usb(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr) {
        return nullptr;
    }
    auto dte = create_usb_dte(dte_config);
    if (dte == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_wrap->dte = dte;
    dce_factory::Factory f(convert_modem_enum(module));
    dce_wrap->dce = f.build(dce_config, std::move(dte), netif);
    if (dce_wrap->dce == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_wrap->modem_type = convert_modem_enum(module);
    dce_wrap->dte_type = esp_modem_dce_wrap::modem_wrap_dte_type::USB;
    return dce_wrap;
}
