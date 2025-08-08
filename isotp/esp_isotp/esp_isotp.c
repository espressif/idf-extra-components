#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_types.h"
#include "esp_check.h"
#include "esp_isotp.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "esp_isotp";
static esp_isotp_handle_t s_isotp_handle = NULL;

/**
 * @brief TWAI receive callback function
 * @note This function runs in ISR context
 */
static IRAM_ATTR bool esp_isotp_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    esp_isotp_handle_t link_handle = (esp_isotp_handle_t) user_ctx;

    ESP_RETURN_ON_FALSE(link_handle != NULL, false, TAG, "Invalid link handle");
    ESP_RETURN_ON_FALSE(link_handle->rx_queue != NULL, false, TAG, "Invalid RX queue");

    BaseType_t higher_priority_task_woken = pdFALSE;
    isotp_rx_queue_item_t queue_item = {0};
    twai_frame_t rx_frame = {
        .buffer = queue_item.data,
        .buffer_len = sizeof(queue_item.data),
    };

    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        if (rx_frame.header.id == link_handle->link.receive_arbitration_id) {
            queue_item.header = rx_frame.header;
            queue_item.data_len = rx_frame.buffer_len;
            xQueueSendFromISR(link_handle->rx_queue, &queue_item, &higher_priority_task_woken);
        }
    }
    return (higher_priority_task_woken == pdTRUE);
}

/**
 * @brief Get current timestamp in milliseconds
 */
uint32_t isotp_user_get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Send CAN frame through TWAI driver
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size)
{
    ESP_RETURN_ON_FALSE(s_isotp_handle != NULL, ISOTP_RET_ERROR, TAG, "Invalid handle");

    twai_frame_t tx_msg = {0};
    tx_msg.header.id = arbitration_id;
    tx_msg.header.ide = false;
    tx_msg.header.rtr = false;
    tx_msg.buffer = (uint8_t *)data;
    tx_msg.buffer_len = size;

    esp_err_t ret = twai_node_transmit(s_isotp_handle->twai_node, &tx_msg, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send CAN frame: %s", esp_err_to_name(ret));
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

/**
 * @brief Debug output function for ISO-TP library
 */
void isotp_user_debug(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    esp_log_writev(ESP_LOG_DEBUG, "isotp_c", message, args);
    va_end(args);
}

esp_isotp_handle_t esp_isotp_new(const esp_isotp_config_t *config)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config != NULL, NULL, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(config->twai_node != NULL, NULL, TAG, "Invalid TWAI node");

    // Allocate memory for handle
    esp_isotp_handle_t handle = calloc(1, sizeof(struct esp_isotp_link));
    ESP_RETURN_ON_FALSE(handle != NULL, NULL, TAG, "Failed to allocate memory for ISO-TP link");
    s_isotp_handle = handle;

    handle->rx_queue = config->rx_queue;
    handle->twai_node = config->twai_node;

    handle->tx_buffer = calloc(config->tx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(handle->tx_buffer, ESP_ERR_NO_MEM, err_tx_buf, TAG, "Failed to allocate memory for TX buffer");

    handle->rx_buffer = calloc(config->rx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(handle->rx_buffer, ESP_ERR_NO_MEM, err_rx_buf, TAG, "Failed to allocate memory for RX buffer");

    // Initialize ISO-TP link with static buffers
    isotp_init_link(&handle->link, config->tx_id, handle->tx_buffer,
                    config->tx_buffer_size, handle->rx_buffer, config->rx_buffer_size);
    handle->link.receive_arbitration_id = config->rx_id;

    // Register TWAI callback
    twai_event_callbacks_t cbs = {
        .on_rx_done = esp_isotp_rx_callback,
    };
    ret = twai_node_register_event_callbacks(handle->twai_node, &cbs, handle);
    ESP_GOTO_ON_ERROR(ret, err_callbacks, TAG, "Failed to register event callbacks: %s", esp_err_to_name(ret));

    // Enable TWAI node
    ret = twai_node_enable(handle->twai_node);
    ESP_GOTO_ON_ERROR(ret, err_enable, TAG, "Failed to enable TWAI node: %s", esp_err_to_name(ret));

    ESP_LOGI(TAG, "New ISO-TP link created, TX: 0x%lX, RX: 0x%lX", config->tx_id, config->rx_id);
    return handle;

err_enable:
err_callbacks:
    free(handle->rx_buffer);
err_rx_buf:
    free(handle->tx_buffer);
err_tx_buf:
    free(handle);
    return NULL;
}

esp_err_t esp_isotp_poll(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Process received queue data
    isotp_rx_queue_item_t rx_item;
    while (xQueueReceive(handle->rx_queue, &rx_item, 0) == pdTRUE) {
        isotp_on_can_message(&handle->link, rx_item.data, rx_item.data_len);
    }

    // Run ISO-TP state machine
    isotp_poll(&handle->link);

    return ESP_OK;
}

int esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint16_t size)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ISOTP_RET_ERROR, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data != NULL, ISOTP_RET_ERROR, TAG, "Invalid data");

    return isotp_send(&handle->link, data, size);
}

int esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint16_t size)
{
    uint16_t out_size = 0;
    ESP_RETURN_ON_FALSE(handle != NULL, ISOTP_RET_ERROR, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data != NULL, ISOTP_RET_ERROR, TAG, "Invalid data");

    int ret = isotp_receive(&handle->link, data, size, &out_size);
    if (ret == ISOTP_RET_OK) {
        return out_size;
    } else if (ret == ISOTP_RET_NO_DATA) {
        return 0;
    }
    return ret;
}

esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Clear callback
    twai_event_callbacks_t cbs = { .on_rx_done = NULL };
    ret = twai_node_register_event_callbacks(handle->twai_node, &cbs, NULL);
    ESP_GOTO_ON_ERROR(ret, clean, TAG, "Failed to unregister callbacks: %s", esp_err_to_name(ret));

    // Disable TWAI node
    ret = twai_node_disable(handle->twai_node);
    ESP_GOTO_ON_ERROR(ret, clean, TAG, "Failed to disable TWAI node: %s", esp_err_to_name(ret));

clean:
    // Free buffers
    if (handle->tx_buffer) {
        free(handle->tx_buffer);
    }
    if (handle->rx_buffer) {
        free(handle->rx_buffer);
    }
    free(handle);
    return ret;
}
