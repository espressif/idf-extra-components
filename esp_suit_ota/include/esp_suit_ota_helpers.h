/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_suit_ota.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*esp_suit_ota_http_init_cb_t)(esp_http_client_handle_t client);

/**
 * @brief Configuration of the built-in HTTPS getter. Points at it from esp_suit_ota_getter_t.cfg.
 */
typedef struct {
    const esp_http_client_config_t *http_config;   /*!< Certificates, timeouts, auth, redirects, buffers */
    esp_suit_ota_http_init_cb_t init_cb;           /*!< Called after every esp_http_client_init(), may add headers */
    size_t max_request_size;                       /*!< 0: whole resource per request; else Range chunks of this size */
} esp_suit_ota_https_cfg_t;

/**
 * @brief Built-in HTTPS getter: reopens on epoch change, Range for resume and chunking,
 *        ETag and If-None-Match for the manifest, relative URIs against the final URL.
 */
int esp_suit_ota_https_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap);
void esp_suit_ota_https_close(void *state);

#ifdef __cplusplus
}
#endif
