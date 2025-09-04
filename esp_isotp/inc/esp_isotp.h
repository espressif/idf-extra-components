/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/**
 * @file esp_isotp.h
 * @brief ISO-TP (ISO 15765-2) Transport Protocol Implementation
 *
 * ISO-TP enables transmission of data larger than 8 bytes over TWAI networks
 * through automatic fragmentation and reassembly.
 *
 * ## How it Works
 *
 * **Small packets (≤7 bytes)**: Sent in a single TWAI frame immediately.
 * **Large packets (>7 bytes)**: Split into multiple frames - first frame sent immediately,
 * remaining frames sent during esp_isotp_poll() calls.
 *
 */

#include "esp_err.h"
#include "esp_twai.h"
#include "esp_twai_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISO-TP link handle
 */
typedef struct esp_isotp_link_t *esp_isotp_handle_t;

/**
 * @brief Configuration structure for creating a new ISO-TP link
 */
typedef struct {
    uint32_t tx_id;                  /*!< TWAI ID for transmitting ISO-TP frames (11-bit or 29-bit) */
    uint32_t rx_id;                  /*!< TWAI ID for receiving ISO-TP frames (11-bit or 29-bit) */
    uint32_t tx_buffer_size;         /*!< Size of the transmit buffer (max message size to send) */
    uint32_t rx_buffer_size;         /*!< Size of the receive buffer (max message size to receive) */
    bool use_extended_id;            /*!< true: use 29-bit extended ID, false: use 11-bit standard ID */
} esp_isotp_config_t;

/**
 * @brief Create a new ISO-TP transport bound to a TWAI node.
 *
 * Allocates internal buffers, creates TX frame pool, registers TWAI callbacks
 * and enables the provided TWAI node.
 *
 * @param twai_node TWAI node handle to bind.
 * @param config Transport configuration.
 * @param[out] out_handle Returned ISO-TP transport handle.
 * @return esp_err_t
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG for invalid parameters
 *  - ESP_ERR_INVALID_SIZE for invalid buffer sizes
 *  - ESP_ERR_NO_MEM when allocation fails
 *  - Other error codes from TWAI functions
 */
esp_err_t esp_isotp_new_transport(twai_node_handle_t twai_node, const esp_isotp_config_t *config, esp_isotp_handle_t *out_handle);

/**
 * @brief Send data over an ISO-TP link (non-blocking)
 *
 * Immediately sends first/single frame and returns. For multi-frame messages,
 * remaining frames are sent during subsequent esp_isotp_poll() calls.
 *
 * @param handle ISO-TP handle
 * @param data Data to send
 * @param size Data length in bytes
 * @return
 *     - ESP_OK: Send initiated successfully
 *     - ESP_ERR_NOT_FINISHED: Previous send still in progress
 *     - ESP_ERR_NO_MEM: Data too large for buffer or no space available
 *     - ESP_ERR_INVALID_SIZE: Invalid data size
 *     - ESP_ERR_TIMEOUT: Send operation timed out
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 *     - ESP_FAIL: Other send errors
 */
esp_err_t esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size);

/**
 * @brief Send data over an ISO-TP link with specified TWAI ID (non-blocking)
 *
 * Similar to esp_isotp_send(), but allows specifying a different TWAI ID for transmission.
 * This function is primarily used for functional addressing where multiple nodes
 * may respond to the same request.
 *
 * @param handle ISO-TP handle
 * @param id TWAI identifier to use for transmission (overrides configured tx_id)
 * @param data Data to send
 * @param size Data length in bytes
 * @return
 *     - ESP_OK: Send initiated successfully
 *     - ESP_ERR_NOT_FINISHED: Previous send still in progress
 *     - ESP_ERR_NO_MEM: Data too large for buffer or no space available
 *     - ESP_ERR_INVALID_SIZE: Invalid data size
 *     - ESP_ERR_TIMEOUT: Send operation timed out
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 *     - ESP_FAIL: Other send errors
 */
esp_err_t esp_isotp_send_with_id(esp_isotp_handle_t handle, uint32_t id, const uint8_t *data, uint32_t size);

/**
 * @brief Extract a complete received message (non-blocking)
 *
 * This function only extracts data that has already been assembled by esp_isotp_poll().
 * It does NOT process incoming TWAI frames - that happens in esp_isotp_poll().
 *
 * Process: TWAI frames → esp_isotp_poll() assembles → esp_isotp_receive() extracts
 *
 * @param handle ISO-TP handle
 * @param data Buffer to store received data
 * @param size Buffer size in bytes
 * @param[out] received_size Actual received data length
 * @return
 *     - ESP_OK: Complete message extracted and internal buffer cleared
 *     - ESP_ERR_NOT_FOUND: No complete message ready for extraction
 *     - ESP_ERR_INVALID_SIZE: Receive buffer overflow or invalid size
 *     - ESP_ERR_INVALID_RESPONSE: Invalid sequence number or protocol error
 *     - ESP_ERR_TIMEOUT: Receive operation timed out
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 *     - ESP_FAIL: Other receive errors
 */
esp_err_t esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint32_t size, uint32_t *received_size);

/**
 * @brief Poll the ISO-TP link to process messages (CRITICAL - call regularly!)
 *
 * This function drives the ISO-TP state machine. Call every 1-10ms for proper operation.
 *
 * What it does:
 * - Sends remaining frames for multi-frame messages
 * - Processes incoming TWAI frames and assembles complete messages
 * - Handles flow control and timeouts
 * - Updates internal state machine
 *
 * Without regular polling: multi-frame sends will stall and receives won't complete.
 *
 * @param handle ISO-TP handle
 * @return
 *     - ESP_OK: Processing successful
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 */
esp_err_t esp_isotp_poll(esp_isotp_handle_t handle);

/**
 * @brief Delete an ISO-TP link
 *
 * @param handle The handle of the ISO-TP link to delete
 * @return
 *     - ESP_OK: Success (or TWAI disable warning logged)
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - Other ESP error codes: TWAI node disable failed
 */
esp_err_t esp_isotp_delete(esp_isotp_handle_t handle);

#ifdef __cplusplus
}
#endif
