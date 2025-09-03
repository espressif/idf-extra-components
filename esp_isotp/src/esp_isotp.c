/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_check.h"
#include "esp_isotp.h"
// Include isotp-c library from submodule
#include "isotp.h"

static const char *TAG = "esp_isotp";

/**
 * @brief ISO-TP link structure
 */
typedef struct esp_isotp_link_t {
    IsoTpLink link;
    twai_node_handle_t twai_node;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    bool use_extended_id;
} esp_isotp_link_t;

/**
 * @brief TWAI receive callback function.
 *
 * Processes a received TWAI frame and feeds it to the ISO-TP state machine.
 *
 * @note Runs in ISR context.
 * @param handle TWAI node handle invoking the callback.
 * @param edata  Receive event data from TWAI driver (unused here).
 * @param user_ctx User context pointer; expected to be an esp_isotp_handle_t.
 * @return true to request a context switch to a higher-priority task, false otherwise.
 */
static IRAM_ATTR bool esp_isotp_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    esp_isotp_handle_t link_handle = (esp_isotp_handle_t) user_ctx;

    uint8_t frame_data[TWAI_FRAME_MAX_LEN];
    twai_frame_t rx_frame = {
        .buffer = frame_data,
        .buffer_len = sizeof(frame_data),
    };

    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        if (rx_frame.header.id == link_handle->link.receive_arbitration_id) {
            // Process received TWAI message and send flow control frames if needed
            isotp_on_can_message(&link_handle->link, frame_data, rx_frame.buffer_len);
        }
    }
    return false;
}

/**
 * @brief Get monotonic timestamp in microseconds.
 *
 * Returns the current time in microseconds as a 32-bit, monotonically
 * increasing value. Wrap-around is expected; the library compares
 * timestamps using IsoTpTimeAfter().
 *
 * @return 32-bit timestamp in microseconds.
 */
uint32_t isotp_user_get_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

/**
 * @brief isotp-c library stub function: send twai message
 *
 * Queues a TWAI frame for transmission using the configured TWAI node.
 *
 * @param arbitration_id CAN identifier (11-bit or 29-bit).
 * @param data Pointer to frame payload.
 * @param size Payload length in bytes (0–8).
 * @param user_data Optional ISO-TP link handle.
 * @retval ISOTP_RET_OK Frame queued successfully.
 * @retval ISOTP_RET_ERROR Transmission failed or invalid context.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size, void *user_data)
{
    esp_isotp_handle_t isotp_handle = (esp_isotp_handle_t) user_data;
    ESP_RETURN_ON_FALSE(isotp_handle != NULL, ISOTP_RET_ERROR, TAG, "Invalid ISO-TP handle");
    twai_node_handle_t twai_node = isotp_handle->twai_node;

    twai_frame_t tx_msg = {0};
    tx_msg.header.id = arbitration_id;
    tx_msg.header.ide = isotp_handle->use_extended_id;
    tx_msg.header.rtr = false;
    tx_msg.buffer = (uint8_t *)data;
    tx_msg.buffer_len = size;

    esp_err_t ret = twai_node_transmit(twai_node, &tx_msg, 0);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send TWAI frame");
    return ISOTP_RET_OK;
}

/**
 * @brief Print a formatted debug message from isotp-c.
 *
 * @param message Format string.
 * @param ... Variadic arguments for the format string.
 */
void isotp_user_debug(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    esp_log_writev(ESP_LOG_DEBUG, "isotp_c", message, args);
    va_end(args);
}

esp_err_t esp_isotp_new_transport(twai_node_handle_t twai_node, const esp_isotp_config_t *config, esp_isotp_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;
    esp_isotp_handle_t isotp = NULL;
    ESP_RETURN_ON_FALSE(twai_node && config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");
    ESP_RETURN_ON_FALSE(config->tx_buffer_size > 0 && config->rx_buffer_size > 0, ESP_ERR_INVALID_SIZE, TAG, "Buffer sizes must be greater than 0");
    ESP_RETURN_ON_FALSE(config->tx_id != config->rx_id, ESP_ERR_INVALID_ARG, TAG, "TX and RX IDs must be different");

    // Validate ID ranges based on type
    uint32_t mask = config->use_extended_id ? TWAI_EXT_ID_MASK : TWAI_STD_ID_MASK;
    ESP_RETURN_ON_FALSE(((config->tx_id & ~mask) == 0) && ((config->rx_id & ~mask) == 0),
                        ESP_ERR_INVALID_ARG, TAG, "ID exceeds mask for selected format");

    // Allocate memory for handle
    isotp = calloc(1, sizeof(esp_isotp_link_t));
    ESP_RETURN_ON_FALSE(isotp, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for ISO-TP link");

    // Allocate memory for buffers
    isotp->tx_buffer = calloc(config->tx_buffer_size, sizeof(uint8_t));
    isotp->rx_buffer = calloc(config->rx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(isotp->rx_buffer && isotp->tx_buffer, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate buffer memory");

    // Initialize ISO-TP link
    isotp_init_link(&isotp->link, config->tx_id, isotp->tx_buffer,
                    config->tx_buffer_size, isotp->rx_buffer, config->rx_buffer_size);
    isotp->link.receive_arbitration_id = config->rx_id;
    isotp->use_extended_id = config->use_extended_id;

    // Set user argument for TWAI operations
    isotp->link.user_send_can_arg = isotp;

    // Register TWAI callback
    twai_event_callbacks_t cbs = {
        .on_rx_done = esp_isotp_rx_callback,
    };
    ret = twai_node_register_event_callbacks(twai_node, &cbs, isotp);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to register event callbacks");

    // Enable TWAI node
    ret = twai_node_enable(twai_node);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to enable TWAI node");

    isotp->twai_node = twai_node;
    *out_handle = isotp;

    return ESP_OK;

err:
    if (isotp) {
        if (isotp->rx_buffer) {
            free(isotp->rx_buffer);
        }
        if (isotp->tx_buffer) {
            free(isotp->tx_buffer);
        }
        free(isotp);
    }
    return ret;
}

esp_err_t esp_isotp_poll(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    // Run ISO-TP state machine to check timeout and send continue frames
    isotp_poll(&handle->link);

    return ESP_OK;
}

esp_err_t esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size)
{
    ESP_RETURN_ON_FALSE(handle && data && size, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    int ret = isotp_send(&handle->link, data, size);
    switch (ret) {
    case ISOTP_RET_OK:
        return ESP_OK;
    case ISOTP_RET_INPROGRESS:
        return ESP_ERR_NOT_FINISHED;
    case ISOTP_RET_OVERFLOW:
    case ISOTP_RET_NOSPACE:
        return ESP_ERR_NO_MEM;
    case ISOTP_RET_LENGTH:
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ISOTP_RET_ERROR:
    default:
        ESP_LOGE(TAG, "ISO-TP send failed with error code: %d", ret);
        return ESP_FAIL;
    }
}

esp_err_t esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint32_t size, uint32_t *received_size)
{
    ESP_RETURN_ON_FALSE(handle && data && size && received_size, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    *received_size = 0;
    int ret = isotp_receive(&handle->link, data, size, received_size);
    switch (ret) {
    case ISOTP_RET_OK:
        return ESP_OK;
    case ISOTP_RET_NO_DATA:
        return ESP_ERR_NOT_FOUND;
    case ISOTP_RET_OVERFLOW:
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_WRONG_SN:
        return ESP_ERR_INVALID_RESPONSE;
    case ISOTP_RET_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ISOTP_RET_LENGTH:
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_ERROR:
    default:
        ESP_LOGE(TAG, "ISO-TP receive failed with error code: %d", ret);
        return ESP_FAIL;
    }
}

esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    esp_err_t ret = ESP_OK;

    // Disable TWAI node (continue cleanup even if this fails)
    esp_err_t twai_ret = twai_node_disable(handle->twai_node);
    if (twai_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable TWAI node: %s", esp_err_to_name(twai_ret));
        ret = twai_ret;
    }

    // Clean up ISO-TP link
    isotp_destroy_link(&handle->link);

    // Free allocated memory
    if (handle->tx_buffer) {
        free(handle->tx_buffer);
    }
    if (handle->rx_buffer) {
        free(handle->rx_buffer);
    }
    free(handle);

    return ret;
}
