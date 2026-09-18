/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_soem.h"
#include <stdlib.h>

/* Initialize SOEM with an ESP-IDF Ethernet driver.
   The returned context is allocated internally and must be released by esp_soem_deinit(). */
ecx_contextt *esp_soem_init(esp_eth_handle_t eth_handle)
{
    if (eth_handle == NULL) {
        return NULL;
    }
    ecx_contextt *context = calloc(1, sizeof(*context));
    if (context == NULL) {
        return NULL;
    }
    context->port.eth_handle = eth_handle;
    if (!ecx_init(context, "esp_eth")) {
        ecx_close(context);
        free(context);
        return NULL;
    }
    return context;
}

void esp_soem_deinit(ecx_contextt *context)
{
    if (context == NULL) {
        return;
    }
    ecx_close(context);
    free(context);
}
