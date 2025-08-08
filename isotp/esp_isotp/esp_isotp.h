#ifndef _ISOTP_ESP_H_
#define _ISOTP_ESP_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_twai.h"
#include "esp_twai_types.h"
#include "isotp-c/isotp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISO-TP link structure
 */
struct esp_isotp_link {
    IsoTpLink link;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    uint16_t tx_buffer_size;
    uint16_t rx_buffer_size;
    QueueHandle_t rx_queue;
    twai_node_handle_t twai_node;
};
typedef struct esp_isotp_link esp_isotp_link_t;
typedef struct esp_isotp_link *esp_isotp_handle_t;

/**
 * @brief Structure to hold received ISO-TP queue item
 */
struct isotp_rx_queue_item {
    twai_frame_header_t header;
    uint8_t data[TWAI_FRAME_MAX_LEN];
    uint8_t data_len;
};
typedef struct isotp_rx_queue_item isotp_rx_queue_item_t;

/**
 * @brief Configuration structure for creating a new ISO-TP link
 */
struct esp_isotp_config {
    uint32_t tx_id;                  /*!< ID for transmitting messages */
    uint32_t rx_id;                  /*!< ID for receiving messages */
    uint16_t tx_buffer_size;
    uint16_t rx_buffer_size;
    twai_node_handle_t twai_node;    /*!< TWAI node handle */
    QueueHandle_t rx_queue;          /*!< Queue for receiving messages */
};
typedef struct esp_isotp_config esp_isotp_config_t;

/**
 * @brief Create a new ISO-TP link
 *
 * @param config Pointer to the configuration structure
 * @return Pointer to the new ISO-TP link, or NULL on error
 */
esp_isotp_handle_t esp_isotp_new(const esp_isotp_config_t *config);

/**
 * @brief Send data over an ISO-TP link
 *
 * @param handle The handle of the ISO-TP link
 * @param data Pointer to the data to send
 * @param size Size of the data to send
 * @return ISOTP_RET_OK on success, ISOTP_RET_ERROR on error
 */
int esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint16_t size);

/**
 * @brief Receive data from an ISO-TP link
 *
 * @param handle The handle of the ISO-TP link
 * @param data Buffer to store the received data
 * @param size Maximum size of the buffer
 * @return Actual size of received data on success, ISOTP_RET_ERROR on error
 */
int esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint16_t size);

/**
 * @brief Poll the ISO-TP link to process messages
 *
 * @param handle The handle of the ISO-TP link
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t esp_isotp_poll(esp_isotp_handle_t handle);

/**
 * @brief Delete an ISO-TP link
 *
 * @param handle The handle of the ISO-TP link to delete
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t esp_isotp_delete(esp_isotp_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _ISOTP_ESP_H_ */
