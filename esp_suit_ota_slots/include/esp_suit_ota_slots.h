/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_suit_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The esp_suit_ota backend built on the declared pairs of this component.
 *
 * Put the result in esp_suit_ota_config_t.slots_ops. Call esp_ota_slots_init() first.
 */
const esp_suit_ota_slots_ops_t *esp_suit_ota_slots_ops(void);

#ifdef __cplusplus
}
#endif
