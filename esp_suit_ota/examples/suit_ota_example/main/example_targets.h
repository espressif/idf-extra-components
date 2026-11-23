/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_suit_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Point the configuration at the targets of this device, and get ready for a campaign.
 *
 * With CONFIG_EXAMPLE_USE_OTA_SLOTS, this declares the pairs and installs the esp_ota_slots
 * backend. Without it, this fills an explicit map of live partitions and installs no backend.
 * Call it once at boot, after nvs_flash_init() and before any read of the data partitions.
 */
esp_err_t example_targets_init(esp_suit_ota_config_t *cfg);

/** @brief The partition that holds the live data of a target. */
const esp_partition_t *example_live_partition(const char *name);

/** @brief At boot, after the self-test passed: cancel the app rollback and settle the record. */
void example_confirm_boot(void);

/** @brief After a data-only release went live: keep it, or put the previous data back. */
void example_settle_data(bool data_ok);

#ifdef __cplusplus
}
#endif
