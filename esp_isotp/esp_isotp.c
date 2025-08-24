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
#include "freertos/FreeRTOS.h"

static const char *TAG = "esp_isotp";

/**
 * @brief ISO-TP link structure
 */
struct esp_isotp_link_t {
    IsoTpLink link;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    twai_node_handle_t twai_node;
};

/**
 * @brief TWAI receive callback function
 * @note This function runs in ISR context
 */
static IRAM_ATTR bool esp_isotp_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    esp_isotp_handle_t link_handle = (esp_isotp_handle_t) user_ctx;

    ESP_RETURN_ON_FALSE_ISR(link_handle != NULL, false, TAG, "Invalid link handle");

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

uint32_t isotp_user_get_us(void)
{
    return (uint32_t)esp_timer_get_time();
}
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_ARG
                        , void *user_data
#endif
                       )
{
#ifdef ISO_TP_USER_SEND_CAN_ARG
    twai_node_handle_t twai_node = (twai_node_handle_t) user_data;
    ESP_RETURN_ON_FALSE(twai_node != NULL, ISOTP_RET_ERROR, TAG, "Invalid TWAI node");
#else
    // Without user_data, we need to get TWAI node from somewhere else
    // This shouldn't happen with current design
    ESP_LOGE(TAG, "No TWAI node available for CAN transmission");
    return ISOTP_RET_ERROR;
#endif

    twai_frame_t tx_msg = {0};
    tx_msg.header.id = arbitration_id;
    tx_msg.header.ide = false;
    tx_msg.header.rtr = false;
    tx_msg.buffer = (uint8_t *)data;
    tx_msg.buffer_len = size;

    esp_err_t ret = twai_node_transmit(twai_node, &tx_msg, 0);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send CAN frame: %s", esp_err_to_name(ret));
    return ISOTP_RET_OK;
}

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
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(twai_node != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid TWAI node");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid output handle");

    // Allocate memory for handle
    esp_isotp_handle_t handle = calloc(1, sizeof(struct esp_isotp_link_t));
    ESP_GOTO_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate memory for ISO-TP link");

    handle->twai_node = twai_node;

    handle->tx_buffer = calloc(config->tx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(handle->tx_buffer != NULL, ESP_ERR_NO_MEM, err_handle, TAG, "Failed to allocate memory for TX buffer");

    handle->rx_buffer = calloc(config->rx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(handle->rx_buffer != NULL, ESP_ERR_NO_MEM, err_tx_buf, TAG, "Failed to allocate memory for RX buffer");

    // Initialize ISO-TP link
    isotp_init_link(&handle->link, config->tx_id, handle->tx_buffer,
                    config->tx_buffer_size, handle->rx_buffer, config->rx_buffer_size);
    handle->link.receive_arbitration_id = config->rx_id;
    
    // Set user argument for TWAI operations
#ifdef ISO_TP_USER_SEND_CAN_ARG
    handle->link.user_send_can_arg = handle->twai_node;
#endif

    // Register TWAI callback
    twai_event_callbacks_t cbs = {
        .on_rx_done = esp_isotp_rx_callback,
    };
    ret = twai_node_register_event_callbacks(handle->twai_node, &cbs, handle);
    ESP_GOTO_ON_ERROR(ret, err_callbacks, TAG, "Failed to register event callbacks: %s", esp_err_to_name(ret));

    // Enable TWAI node
    ret = twai_node_enable(handle->twai_node);
    ESP_GOTO_ON_ERROR(ret, err_enable, TAG, "Failed to enable TWAI node: %s", esp_err_to_name(ret));

    *out_handle = handle;
    return ESP_OK;

err_callbacks:
err_enable:
    free(handle->rx_buffer);
err_tx_buf:
    free(handle->tx_buffer);
err_handle:
    free(handle);
err:
    *out_handle = NULL;
    return ret;
}

esp_err_t esp_isotp_poll(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Run ISO-TP state machine to check timeout and send continue frames
    isotp_poll(&handle->link);

    return ESP_OK;
}

esp_err_t esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid data");

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
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid data");
    ESP_RETURN_ON_FALSE(received_size != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid received_size pointer");

    int ret = isotp_receive(&handle->link, data, size, received_size);
    if (ret == ISOTP_RET_OK) {
        return ESP_OK;
    } else if (ret == ISOTP_RET_NO_DATA) {
        *received_size = 0;
        return ESP_ERR_NOT_FOUND;
    }
    *received_size = 0;
    return ESP_FAIL;
}

esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Disable TWAI node
    ret = twai_node_disable(handle->twai_node);
    ESP_GOTO_ON_ERROR(ret, clean, TAG, "Failed to disable TWAI node: %s", esp_err_to_name(ret));

clean:
    isotp_destroy_link(&handle->link);
    free(handle->tx_buffer);
    free(handle->rx_buffer);
    free(handle);
    return ret;
}

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
esp_err_t esp_isotp_set_tx_done_callback(esp_isotp_handle_t handle, 
                                         void (*callback)(void *link, uint32_t tx_size, void *user_arg), 
                                         void *user_arg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    
    isotp_set_tx_done_cb(&handle->link, callback, user_arg);
    return ESP_OK;
}
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
esp_err_t esp_isotp_set_rx_done_callback(esp_isotp_handle_t handle, 
                                         void (*callback)(void *link, const uint8_t *data, uint32_t size, void *user_arg), 
                                         void *user_arg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    
    isotp_set_rx_done_cb(&handle->link, callback, user_arg);
    return ESP_OK;
}
#endif
