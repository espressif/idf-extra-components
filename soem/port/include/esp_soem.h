/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_driver.h"
#include "soem/soem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate a SOEM context and install the SOEM raw-frame Rx callback
   on the supplied Ethernet driver. The Ethernet driver remains owned by the application.
   @param eth_handle, Handle of an installed ESP-IDF Ethernet driver.
   @return Pointer to the allocated SOEM context, or NULL on failure. */
ecx_contextt *esp_soem_init(esp_eth_handle_t eth_handle);

/* Release a SOEM context.
   @param context, SOEM context returned by esp_soem_init(). May be NULL. */
void esp_soem_deinit(ecx_contextt *context);

#ifdef __cplusplus
}
#endif
