/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_suit_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Start NimBLE and advertise the payload service. Call once, before the OTA loop. */
esp_err_t example_ble_payload_start(void);

/** @brief Getter for a "ble://" URI: ask the peer for the resource, return what it writes. */
int example_ble_payload_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap);

/** @brief Release the getter state. The stack keeps running. */
void example_ble_payload_close(void *state);

#ifdef __cplusplus
}
#endif
