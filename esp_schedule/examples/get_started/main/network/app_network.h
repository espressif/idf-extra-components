/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "network_provisioning/manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event group bits for network provisioning states */
#define NETWORK_CONNECTED_BIT      BIT0
#define NETWORK_DISCONNECTED_BIT   BIT1
#define PROVISIONING_SUCCESS_BIT   BIT2
#define PROVISIONING_FAILED_BIT    BIT3
#define TIME_SYNC_SUCCESS_BIT      BIT4
#define TIME_SYNC_FAILED_BIT       BIT5

/**
 * @brief Initialize network provisioning with event group handling
 *
 * This function initializes the network provisioning system and sets up event handlers
 * for network provisioning and IP events. It uses either BLE or SoftAP provisioning
 * based on the Kconfig selection.
 *
 * @param event_group Pointer to the event group that will be used for synchronization
 * @return esp_err_t ESP_OK on success, or error code on failure
 */
esp_err_t app_network_init(EventGroupHandle_t event_group);

/**
 * @brief Start network provisioning and wait for connection
 *
 * This function checks if the device is already provisioned. If provisioned,
 * it starts WiFi station and waits for network connection. If not provisioned,
 * it starts the provisioning sequence and waits for network connection.
 *
 * @param event_group The event group to wait on
 * @param timeout_ms Timeout in milliseconds
 * @return esp_err_t ESP_OK if network connected, ESP_FAIL if failed or timed out
 */
esp_err_t app_network_start(EventGroupHandle_t event_group, uint32_t timeout_ms);

/**
 * @brief Start time synchronization
 *
 * This function starts SNTP time synchronization and waits for it to complete.
 *
 * @param event_group The event group to signal completion on
 */
void app_network_start_time_sync(EventGroupHandle_t event_group);

/**
 * @brief Wait for time synchronization to complete
 *
 * This function waits for time synchronization to complete successfully.
 *
 * @param event_group The event group to wait on
 * @param timeout_ms Timeout in milliseconds
 * @return esp_err_t ESP_OK if time sync succeeded, ESP_FAIL if failed or timed out
 */
esp_err_t app_network_wait_for_time_sync(EventGroupHandle_t event_group, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
