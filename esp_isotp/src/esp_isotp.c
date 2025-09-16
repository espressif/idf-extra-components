/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_isotp.h"
// Include isotp-c library from submodule
#include "isotp.h"

static const char *TAG = "esp_isotp";

/**
 * @brief Determine if the given ID requires extended (29-bit) format
 *
 * @param id TWAI identifier to check
 * @return true if ID requires 29-bit extended format, false for 11-bit standard format
 */
static inline bool is_extended_id(uint32_t id)
{
    return (id > TWAI_STD_ID_MASK);
}

/**
 * @brief TWAI frame container with embedded data buffer.
 *
 * This structure wraps the TWAI frame with an embedded 8-byte data buffer
 * to ensure memory safety during asynchronous operations.
 *
 * Used for:
 * - TX frames: Pre-allocated in SLIST pool, recycled after transmission
 * - RX frames: Pre-allocated in link structure for ISR-safe reception
 */
typedef struct esp_isotp_frame_t {
    twai_frame_t frame;           ///< TWAI driver frame structure
    uint8_t data_payload[8];      ///< Embedded 8-byte TWAI frame data buffer
    SLIST_ENTRY(esp_isotp_frame_t) link;  ///< Single-linked list entry for frame pool
} esp_isotp_frame_t;

SLIST_HEAD(frame_pool_head, esp_isotp_frame_t);

/**
 * @brief ISO-TP link context structure.
 *
 * Contains all state and buffers needed for an ISO-TP transport session.
 * This structure bridges the isotp-c library with ESP-IDF TWAI driver.
 */
typedef struct esp_isotp_link_t {
    IsoTpLink link;                           ///< isotp-c library link state
    twai_node_handle_t twai_node;             ///< Associated TWAI driver node handle
    uint8_t *isotp_tx_buffer;                 ///< ISO-TP TX reassembly buffer (for multi-frame messages)
    uint8_t *isotp_rx_buffer;                 ///< ISO-TP RX reassembly buffer (for multi-frame messages)
    esp_isotp_frame_t isr_rx_frame_buffer;    ///< Pre-allocated frame buffer for ISR-safe RX operations
    struct frame_pool_head tx_frame_pool;     ///< Single-linked list of available TX frames
    esp_isotp_frame_t *tx_frame_array;        ///< Pre-allocated array of TX frames
    size_t tx_frame_pool_size;                ///< Size of TX frame pool
    esp_isotp_rx_callback_t rx_callback;      ///< User RX callback function
    esp_isotp_tx_callback_t tx_callback;      ///< User TX callback function
    void *callback_arg;                       ///< User argument for callbacks
} esp_isotp_link_t;

/**
 * @brief Wrapper callback for isotp-c RX completion.
 *
 * This function wraps the user's callback to hide isotp-c internal link parameter.
 *
 * @note Execution context: This callback runs in the same context as isotp_on_can_message(),
 *       which is typically called from ISR context (esp_isotp_rx_callback).
 *       Therefore, user RX callbacks should avoid blocking operations and keep execution minimal.
 *
 * @param link ISO-TP link handle (unused).
 * @param data Pointer to received data.
 * @param size Size of received data in bytes.
 * @param user_arg User context pointer (esp_isotp_handle_t).
 */
#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
static void esp_isotp_rx_wrapper(void *link, const uint8_t *data, uint32_t size, void *user_arg)
{
    esp_isotp_handle_t handle = (esp_isotp_handle_t)user_arg;
    if (handle && handle->rx_callback) {
        handle->rx_callback(handle, data, size, handle->callback_arg);
    }
}
#endif

/**
 * @brief Wrapper callback for isotp-c TX completion.
 *
 * This function wraps the user's callback to hide isotp-c internal link parameter.
 *
 * @note Execution context depends on transmission type:
 *       - Single-frame: Called from isotp_send() in the same context as the caller
 *         (may be ISR context if esp_isotp_send() was called from ISR)
 *       - Multi-frame: Called from isotp_poll() in task context when the last frame is sent
 *
 * @param link ISO-TP link handle (unused).
 * @param tx_size Size of transmitted data in bytes.
 * @param user_arg User context pointer (esp_isotp_handle_t).
 */
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
static void esp_isotp_tx_wrapper(void *link, uint32_t tx_size, void *user_arg)
{
    esp_isotp_handle_t handle = (esp_isotp_handle_t)user_arg;
    if (handle && handle->tx_callback) {
        handle->tx_callback(handle, tx_size, handle->callback_arg);
    }
}
#endif

/**
 * @brief TWAI transmit done callback.
 *
 * Called when a TWAI frame transmission is complete. Returns the used
 * frame back to the SLIST pool for reuse.
 *
 * @note Runs in ISR context.
 * @param handle TWAI node handle invoking the callback.
 * @param edata Transmit event data from TWAI driver.
 * @param user_ctx User context pointer (esp_isotp_handle_t).
 * @return Always returns false (no context switch needed).
 */
static IRAM_ATTR bool esp_isotp_tx_callback(twai_node_handle_t handle, const twai_tx_done_event_data_t *edata, void *user_ctx)
{
    esp_isotp_handle_t isotp_handle = (esp_isotp_handle_t) user_ctx;
    // Return the used frame back to the SLIST pool.
    if (isotp_handle && edata->done_tx_frame) {
        esp_isotp_frame_t *tx_frame = (esp_isotp_frame_t *)edata->done_tx_frame;
        // Return frame to SLIST pool for reuse
        SLIST_INSERT_HEAD(&isotp_handle->tx_frame_pool, tx_frame, link);
    }

    return false;
}

/**
 * @brief TWAI receive done callback.
 *
 * Processes a received TWAI frame and feeds it to the ISO-TP state machine.
 *
 * @note Runs in ISR context.
 * @param handle TWAI node handle invoking the callback.
 * @param edata Receive event data from TWAI driver (unused).
 * @param user_ctx User context pointer (esp_isotp_handle_t).
 * @return true to request a context switch to a higher-priority task, false otherwise.
 */
static IRAM_ATTR bool esp_isotp_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    esp_isotp_handle_t link_handle = (esp_isotp_handle_t)user_ctx;
    if (!link_handle) {
        return false;  // No valid context, nothing to do
    }

    esp_isotp_frame_t *rx_frame = &link_handle->isr_rx_frame_buffer;

    if (twai_node_receive_from_isr(handle, &rx_frame->frame) != ESP_OK) {
        return false;
    }

    // ID match check
    if (rx_frame->frame.header.id != link_handle->link.receive_arbitration_id) {
        return false;
    }

    // Feed received TWAI frame to isotp-c state machine for reassembly.
    // isotp-c will handle single/multi-frame logic and send flow control frames as needed.
    isotp_on_can_message(&link_handle->link, rx_frame->frame.buffer, rx_frame->frame.buffer_len);

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
 * @param arbitration_id TWAI identifier (11-bit or 29-bit).
 * @param data Pointer to frame payload.
 * @param size Payload length in bytes (0–8).
 * @param user_data Optional ISO-TP link handle.
 * @retval ISOTP_RET_OK Frame queued successfully.
 * @retval ISOTP_RET_ERROR Transmission failed or invalid context.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size, void *user_data)
{
    esp_isotp_handle_t isotp_handle = (esp_isotp_handle_t) user_data;
    ESP_RETURN_ON_FALSE_ISR(isotp_handle != NULL, ISOTP_RET_ERROR, TAG, "Invalid ISO-TP handle");

    // Size validation - TWAI frames are max 8 bytes by protocol
    ESP_RETURN_ON_FALSE_ISR(size <= 8, ISOTP_RET_ERROR, TAG, "Invalid TWAI frame size");

    twai_node_handle_t twai_node = isotp_handle->twai_node;

    // Get a pre-allocated frame from the SLIST pool.
    // This avoids dynamic allocation overhead completely.
    esp_isotp_frame_t *tx_frame = SLIST_FIRST(&isotp_handle->tx_frame_pool);
    ESP_RETURN_ON_FALSE_ISR(tx_frame != NULL, ISOTP_RET_ERROR, TAG, "No available frames in pool");

    // Remove frame from pool
    SLIST_REMOVE_HEAD(&isotp_handle->tx_frame_pool, link);

    // Initialize TWAI frame header and copy payload data into embedded buffer.
    memset(&tx_frame->frame, 0, sizeof(twai_frame_t));
    tx_frame->frame.header.id = arbitration_id;
    tx_frame->frame.header.ide = is_extended_id(arbitration_id);  // Extended (29-bit) vs Standard (11-bit) ID

    // Copy payload into the embedded buffer to ensure data lifetime during async transmission.
    memcpy(tx_frame->data_payload, data, size);

    tx_frame->frame.buffer = tx_frame->data_payload;
    tx_frame->frame.buffer_len = size;

    // Send the frame; TX callback will return frame to pool on completion.
    esp_err_t ret = twai_node_transmit(twai_node, &tx_frame->frame, 0);
    if (ret != ESP_OK) {
        // Return frame to SLIST pool if sending failed immediately.
        SLIST_INSERT_HEAD(&isotp_handle->tx_frame_pool, tx_frame, link);
        ESP_EARLY_LOGE(TAG, "Failed to send TWAI frame: %s", esp_err_to_name(ret));
        return ISOTP_RET_ERROR;
    }

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
    ESP_RETURN_ON_FALSE(config->tx_frame_pool_size != 0, ESP_ERR_INVALID_SIZE, TAG, "TX frame pool size cannot be zero");

    // Validate ID ranges - each ID is validated against its own required format
    ESP_RETURN_ON_FALSE((config->tx_id & ~TWAI_EXT_ID_MASK) == 0,
                        ESP_ERR_INVALID_ARG, TAG, "TX ID exceeds maximum value");
    ESP_RETURN_ON_FALSE((config->rx_id & ~TWAI_EXT_ID_MASK) == 0,
                        ESP_ERR_INVALID_ARG, TAG, "RX ID exceeds maximum value");

    // Allocate memory for handle.
    isotp = calloc(1, sizeof(esp_isotp_link_t));
    ESP_RETURN_ON_FALSE(isotp, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for ISO-TP link");

    // Allocate ISO-TP reassembly buffers for multi-frame message handling.
    // These buffers are used by isotp-c to reassemble/fragment large payloads.
    isotp->isotp_tx_buffer = calloc(config->tx_buffer_size, sizeof(uint8_t));
    isotp->isotp_rx_buffer = calloc(config->rx_buffer_size, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(isotp->isotp_rx_buffer && isotp->isotp_tx_buffer, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate ISO-TP reassembly buffers");

    // Initialize TX frame pool with user-specified size
    // Using simple single-linked list for maximum efficiency
    isotp->tx_frame_pool_size = config->tx_frame_pool_size;
    SLIST_INIT(&isotp->tx_frame_pool);

    // Allocate array of TX frames
    isotp->tx_frame_array = calloc(isotp->tx_frame_pool_size, sizeof(esp_isotp_frame_t));
    ESP_GOTO_ON_FALSE(isotp->tx_frame_array, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate TX frame array");

    // Initialize each frame and add to SLIST pool
    for (size_t i = 0; i < isotp->tx_frame_pool_size; i++) {
        esp_isotp_frame_t *frame = &isotp->tx_frame_array[i];
        frame->frame.buffer = frame->data_payload;
        frame->frame.buffer_len = sizeof(frame->data_payload);

        SLIST_INSERT_HEAD(&isotp->tx_frame_pool, frame, link);
    }

    // Initialize the isotp-c library link with our allocated buffers.
    isotp_init_link(&isotp->link, config->tx_id, isotp->isotp_tx_buffer,
                    config->tx_buffer_size, isotp->isotp_rx_buffer, config->rx_buffer_size);
    isotp->link.receive_arbitration_id = config->rx_id;

    // Pre-allocate ISR-safe receive frame buffer to avoid dynamic allocation in interrupt context.
    // This buffer is reused for each incoming TWAI frame received in the ISR.
    memset(&isotp->isr_rx_frame_buffer, 0, sizeof(esp_isotp_frame_t));
    isotp->isr_rx_frame_buffer.frame.buffer = isotp->isr_rx_frame_buffer.data_payload;
    isotp->isr_rx_frame_buffer.frame.buffer_len = sizeof(isotp->isr_rx_frame_buffer.data_payload);

    // Set user argument for TWAI operations.
    isotp->link.user_send_can_arg = isotp;

    // Save user callback functions
    isotp->rx_callback = config->rx_callback;
    isotp->tx_callback = config->tx_callback;
    isotp->callback_arg = config->callback_arg;

    // Set isotp-c wrapper callbacks if user callbacks provided.
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    if (config->tx_callback) {
        isotp_set_tx_done_cb(&isotp->link, esp_isotp_tx_wrapper, isotp);
    }
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    if (config->rx_callback) {
        isotp_set_rx_done_cb(&isotp->link, esp_isotp_rx_wrapper, isotp);
    }
#endif

    // Register TWAI callbacks.
    twai_event_callbacks_t cbs = {
        .on_rx_done = esp_isotp_rx_callback,
        .on_tx_done = esp_isotp_tx_callback,
    };
    ret = twai_node_register_event_callbacks(twai_node, &cbs, isotp);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to register event callbacks");

    // Enable TWAI node.
    ret = twai_node_enable(twai_node);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to enable TWAI node");

    isotp->twai_node = twai_node;
    *out_handle = isotp;

    return ESP_OK;

err:
    if (isotp) {
        if (isotp->isotp_rx_buffer) {
            free(isotp->isotp_rx_buffer);
        }
        if (isotp->isotp_tx_buffer) {
            free(isotp->isotp_tx_buffer);
        }
        if (isotp->tx_frame_array) {
            free(isotp->tx_frame_array);
        }
        free(isotp);
    }
    return ret;
}

/**
 * @brief Delete an ISO-TP transport and free resources.
 *
 * Disables the TWAI node, cleans up TX frame pool and frees allocated
 * memory. Continues cleanup even if disabling TWAI fails.
 *
 * @param handle ISO-TP transport handle.
 * @return ESP_OK on success or the error from TWAI disable.
 */
esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    esp_err_t ret = ESP_OK;

    // Disable TWAI node after unregistering callbacks
    esp_err_t twai_ret = twai_node_disable(handle->twai_node);
    if (twai_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable TWAI node: %s", esp_err_to_name(twai_ret));
        if (ret == ESP_OK) {
            ret = twai_ret;
        }
    }

    // Unregister TWAI callbacks first to prevent use-after-free during disable
    twai_event_callbacks_t cbs = { 0 };
    esp_err_t unreg_ret = twai_node_register_event_callbacks(handle->twai_node, &cbs, NULL);
    if (unreg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister TWAI callbacks: %s", esp_err_to_name(unreg_ret));
        ret = unreg_ret;
    }

    // Clean up ISO-TP link.
    isotp_destroy_link(&handle->link);

    // Clean up TX frame array (SLIST pool is automatically cleaned when frames are freed).
    if (handle->tx_frame_array) {
        free(handle->tx_frame_array);
    }

    // Free ISO-TP reassembly buffers and handle.
    if (handle->isotp_tx_buffer) {
        free(handle->isotp_tx_buffer);
    }
    if (handle->isotp_rx_buffer) {
        free(handle->isotp_rx_buffer);
    }
    free(handle);

    return ret;
}


/**
 * @brief Poll the ISO-TP link. Call this periodically from a task.
 *
 * @param handle ISO-TP transport handle.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t esp_isotp_poll(esp_isotp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid parameters");

    // Run ISO-TP state machine to check timeouts and send consecutive frames.
    isotp_poll(&handle->link);

    return ESP_OK;
}

/**
 * @brief Send a payload using ISO-TP.
 *
 * @param handle ISO-TP transport handle.
 * @param data Pointer to payload buffer.
 * @param size Payload size in bytes.
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_NOT_FINISHED when the send is still in progress
 *  - ESP_ERR_NO_MEM for buffer overflow conditions
 *  - ESP_ERR_INVALID_SIZE for invalid sizes
 *  - ESP_ERR_TIMEOUT on timeout
 *  - ESP_FAIL for other errors
 */
esp_err_t esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size)
{
    if (!(handle && data && size)) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = isotp_send(&handle->link, data, size);
    switch (ret) {
    case ISOTP_RET_OK:
        return ESP_OK;
    case ISOTP_RET_INPROGRESS:
        return ESP_ERR_NOT_FINISHED;
    case ISOTP_RET_OVERFLOW:    // Buffer overflow
    case ISOTP_RET_NOSPACE:     // Not enough space in internal buffers
        return ESP_ERR_NO_MEM;
    case ISOTP_RET_LENGTH:      // Payload size exceeds buffer size
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ISOTP_RET_ERROR:
    default:
        ESP_EARLY_LOGE(TAG, "ISO-TP send failed with error code: %d", ret);
        return ESP_FAIL;
    }
}

/**
 * @brief Send a payload with a specific TWAI ID.
 *
 * Uses the provided TWAI ID for this transmission instead of the default.
 *
 * @param handle ISO-TP transport handle.
 * @param id TWAI identifier (subject to STD/EXT mask based on configuration).
 * @param data Pointer to payload buffer.
 * @param size Payload size in bytes.
 * @return Same as esp_isotp_send().
 */
esp_err_t esp_isotp_send_with_id(esp_isotp_handle_t handle, uint32_t id, const uint8_t *data, uint32_t size)
{
    if (!(handle && data && size)) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((id & ~TWAI_EXT_ID_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = isotp_send_with_id(&handle->link, id, data, size);
    switch (ret) {
    case ISOTP_RET_OK:
        return ESP_OK;
    case ISOTP_RET_INPROGRESS:
        return ESP_ERR_NOT_FINISHED;
    case ISOTP_RET_OVERFLOW:    // Buffer overflow
    case ISOTP_RET_NOSPACE:     // Not enough space in internal buffers
        return ESP_ERR_NO_MEM;
    case ISOTP_RET_LENGTH:      // Payload size exceeds buffer size
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ISOTP_RET_ERROR:
    default:
        ESP_EARLY_LOGE(TAG, "ISO-TP send with ID failed with error code: %d", ret);
        return ESP_FAIL;
    }
}

/**
 * @brief Receive a payload using ISO-TP.
 *
 * @param handle ISO-TP transport handle.
 * @param data Output buffer for received data.
 * @param size Size of the output buffer in bytes.
 * @param received_size Actual number of bytes written to the buffer.
 * @return
 *  - ESP_OK when data is received
 *  - ESP_ERR_NOT_FOUND when no data is available
 *  - ESP_ERR_INVALID_SIZE when the buffer is too small or size invalid
 *  - ESP_ERR_INVALID_RESPONSE on sequence errors
 *  - ESP_ERR_TIMEOUT on timeout
 *  - ESP_FAIL for other errors
 */
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
    case ISOTP_RET_OVERFLOW:    // Receive buffer too small for the message
    case ISOTP_RET_LENGTH:      // Invalid length in message
        return ESP_ERR_INVALID_SIZE;
    case ISOTP_RET_NOSPACE:     // Not enough space in internal buffers (rare if configured correctly)
        return ESP_ERR_NO_MEM;
    case ISOTP_RET_WRONG_SN:    // Sequence number error
        return ESP_ERR_INVALID_RESPONSE;
    case ISOTP_RET_INPROGRESS:  // Should not happen in receive, but handle for robustness
        return ESP_ERR_INVALID_STATE;
    case ISOTP_RET_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ISOTP_RET_ERROR:
    default:
        ESP_LOGE(TAG, "ISO-TP receive failed with error code: %d", ret);
        return ESP_FAIL;
    }
}
