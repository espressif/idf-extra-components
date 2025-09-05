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
} esp_isotp_link_t;

/**
 * @brief TWAI receive callback function
 * @note This function runs in ISR context
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

/// isotp-c library stub function: gets the amount of time passed since the last call in microseconds
uint32_t isotp_user_get_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

/// isotp-c library stub function: send twai message
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size, void *user_data)
{
    twai_node_handle_t twai_node = (twai_node_handle_t) user_data;
    ESP_RETURN_ON_FALSE(twai_node != NULL, ISOTP_RET_ERROR, TAG, "Invalid TWAI node");

    twai_frame_t tx_msg = {0};
    tx_msg.header.id = arbitration_id;
    tx_msg.header.ide = false;
    tx_msg.header.rtr = false;
    tx_msg.buffer = (uint8_t *)data;
    tx_msg.buffer_len = size;

    esp_err_t ret = twai_node_transmit(twai_node, &tx_msg, 0);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send TWAI frame");
    return ISOTP_RET_OK;
}

/// isotp-c library stub function: print debug message
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

    isotp->link.user_send_can_arg = twai_node;

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
    if (isotp->rx_buffer) {
        free(isotp->rx_buffer);
    }
    if (isotp->tx_buffer) {
        free(isotp->tx_buffer);
    }
    free(isotp);
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
    if (ret == ISOTP_RET_OK) {
        return ESP_OK;
    } else if (ret == ISOTP_RET_INPROGRESS) {
        return ESP_ERR_NOT_FINISHED;
    } else if (ret == ISOTP_RET_OVERFLOW) {
        return ESP_ERR_NO_MEM;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint32_t size, uint32_t *received_size)
{
    ESP_RETURN_ON_FALSE(handle && data && size && received_size, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    int ret = isotp_receive(&handle->link, data, size, received_size);
    if (ret == ISOTP_RET_OK) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    // Disable TWAI node
    ESP_RETURN_ON_ERROR(twai_node_disable(handle->twai_node), TAG, "Failed to disable TWAI node");

    isotp_destroy_link(&handle->link);
    free(handle->tx_buffer);
    free(handle->rx_buffer);
    free(handle);
    return ESP_OK;
}
