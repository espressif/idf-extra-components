/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The abstraction of the low-level communication channel used by X/YMODEM protocol layer.
 */
typedef struct {
    /**
     * @brief Receive data through some interface, such as a serial port.
     *
     * @param ctx[in] User-defined context.
     * @param buf[out] Buffer to store received data.
     * @param len Requested length of data to receive.
     * @param recv_len[out] Actual length of data received.
     * @param timeout_ms Timeout in milliseconds. 0 means non-blocking. Negative values means wait forever.
     *
     * @return
     *   - ESP_OK if the requested amount of data is received in full, or if the timeout expires.
     *   - others if an error occurs during the transfer. Timeout is NOT considered as an error.
     *
     * @note Even if the requested amount of data is not fully received before the timeout, or no
     *       data is received at all, return ESP_OK and provide whatever data has been received.
     */
    esp_err_t (*recv)(void *ctx, void *buf, size_t len, size_t *recv_len, int32_t timeout_ms);
    /**
     * @brief Send data through some interface, such as a serial port.
     *
     * @param ctx[in] User-defined context.
     * @param buf[in] Buffer to send.
     * @param len Length of data to send.
     *
     * @return
     *   - ESP_OK if all data was successfully sent or queued for transmission.
     *   - others if an error occurs during the transfer.
     */
    esp_err_t (*send)(void *ctx, const void *buf, size_t len);
    void *ctx;
} xymodem_transport_t;

/**
 * @brief Read data from a source, such as a file or a simple array.
 *        The protocol layer obtains the data to be sent through this function.
 *
 * @param ctx[in] User-defined context.
 * @param buf[out] Buffer to store read data.
 * @param len Requested length of data to read.
 * @param read_len[out] Actual length of data read.
 *
 * @return ESP_OK if the data was successfully read. Returning an error will abort the X/YMODEM transfer.
 *
 * @note This function attempts to read exactly len bytes, but may return fewer bytes if the end of the source is reached.
 *       A short read (read_len < len) is only allowed when no further data is available.
 */
typedef esp_err_t (*xymodem_read_fn_t)(void *ctx, void *buf, size_t len, size_t *read_len);

/**
 * @brief Write data to a destination, such as a file or a simple array.
 *        The data received by the protocol layer is passed to the upper-layer application through this function.
 *
 * @param ctx[in] User-defined context.
 * @param buf[in] Buffer containing the data to write.
 * @param len Length of data to write.
 *
 * @return ESP_OK if all data was successfully written. Returning an error will abort the X/YMODEM transfer.
 *
 * @note This function can also process the data on the fly, without necessarily storing it anywhere.
 */
typedef esp_err_t (*xymodem_write_fn_t)(void *ctx, const void *buf, size_t len);

/**
 * @brief The abstraction representing the source of data to be sent via XMODEM.
 */
typedef struct {
    xymodem_read_fn_t read;
    void *ctx;
} xmodem_data_source_t;

/**
 * @brief The abstraction representing where data received by XMODEM is delivered.
 */
typedef struct {
    xymodem_write_fn_t write;
    void *ctx;
} xmodem_data_sink_t;

/**
 * @brief File metadata as defined by the YMODEM protocol.
 * @note Additional fields such as `mtime` and `mode` may be added in the future.
 */
typedef struct {
    const char *filename;
    size_t file_size;
} ymodem_file_info_t;

/**
 * @brief The abstraction representing the source of files to be sent via YMODEM.
 */
typedef struct {
    /**
     * @brief Provide the protocol layer with the metadata of the next file to be sent.
     *
     * @param ctx[in] User-defined context.
     * @param file_info[out] Metadata of the next file to be sent. Set `filename` to NULL or empty string to indicate no more files to send.
     *                       The provided `filename` pointer must remain valid until the corresponding `end()` is called.
     *
     * @return ESP_OK if the metadata was successfully provided. Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*next)(void *ctx, ymodem_file_info_t *file_info);
    /**
     * @brief Read data from the file provided by the most recent `next()` call.
     */
    xymodem_read_fn_t read;
    /**
     * @brief Called by the protocol layer to notify the upper-layer application that the transfer of the file provided by the most recent next() call has completed.
     *        The user may release associated resources in this callback, such as closing the file.
     *
     * @param ctx[in] User-defined context.
     * @param result Result of the transfer.
     *
     * @return ESP_OK if the upper-layer application acknowledges the information and successfully performs any necessary operations.
     *         Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*end)(void *ctx, esp_err_t result);
    void *ctx;
} ymodem_file_source_t;

/**
 * @brief The abstraction representing where files received by YMODEM are delivered.
 */
typedef struct {
    /**
     * @brief Called by the protocol layer to notify the upper-layer application of the metadata of the incoming file.
     *        The user may use this callback to prepare for the reception of the file, such as creating a file.
     *
     * @param ctx[in] User-defined context.
     * @param file_info[in] Metadata of the incoming file. The `filename` pointer is only guaranteed to be valid for the duration of this callback.
     *
     * @return ESP_OK if the upper-layer application accepts the information and successfully performs any necessary operations.
     *         Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*next)(void *ctx, const ymodem_file_info_t *file_info);
    /**
     * @brief Write data to the file specified by the most recent next() call.
     */
    xymodem_write_fn_t write;
    /**
     * @brief Called by the protocol layer to notify the upper-layer application that the transfer of the file specified by the most recent next() call has completed.
     *        The user may release associated resources in this callback, such as closing the file.
     *
     * @param ctx[in] User-defined context.
     * @param result Result of the transfer.
     *
     * @return ESP_OK if the upper-layer application acknowledges the information and successfully performs any necessary operations.
     *         Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*end)(void *ctx, esp_err_t result);
    void *ctx;
} ymodem_file_sink_t;

/**
 * @brief Verification types as defined by the X/YMODEM protocol.
 */
typedef enum {
    XYMODEM_CHECK_AUTO = 0,  // Use CRC16 by default, and fall back to SUM if the peer does not support it.
    XYMODEM_CHECK_SUM8,
    XYMODEM_CHECK_CRC16,
} xymodem_check_type_t;

/**
 * @brief Block size as defined by the X/YMODEM protocol.
 */
typedef enum {
    XYMODEM_BLOCK_SIZE_MIXED = 0,    // Use 1024-byte blocks by default, and switch to 128-byte blocks if the remaining data is no more than 128 bytes.
    XYMODEM_BLOCK_SIZE_128 = 128,    // Use 128-byte blocks only.
    XYMODEM_BLOCK_SIZE_1024 = 1024,  // Use 1024-byte blocks only.
} xymodem_block_size_t;

typedef struct {
    int32_t timeout_ms;
    int32_t max_retry_cnt;
    xymodem_block_size_t block_size;
} xymodem_send_config_t;

typedef struct {
    int32_t timeout_ms;
    int32_t max_retry_cnt;
    xymodem_check_type_t check_type;
} xymodem_recv_config_t;

/**************************************
 * Generic APIs
 **************************************/

esp_err_t xmodem_send(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const xmodem_data_source_t *source);
esp_err_t xmodem_recv(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, const xmodem_data_sink_t *sink);

esp_err_t ymodem_send(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const ymodem_file_source_t *source);
esp_err_t ymodem_recv(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, const ymodem_file_sink_t *sink);

/**************************************
 * Convenience APIs
 * implemented on top of the generic APIs
 **************************************/

/**
 * @brief Directly send the contents of a buffer using XMODEM.
 *
 * @param transport[in] The transport layer to use.
 * @param config[in] The send configuration.
 * @param buf[in] The buffer to send.
 * @param len The length of the buffer.
 *
 * @return ESP_OK if the data was successfully sent.
 */
esp_err_t xmodem_send_buffer(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const void *buf, size_t len);

/**
 * @brief Directly receive data into a buffer using XMODEM.
 *
 * @param transport[in] The transport layer to use.
 * @param config[in] The receive configuration.
 * @param buf[out] The buffer to receive the data.
 * @param len The length of the buffer.
 * @param recv_len[out] The actual length of the data received.
 *
 * @note The amount of data received via XMODEM is determined by the peer and may exceed the buffer size. Any excess data will be discarded.
 *       Also note that XMODEM cannot distinguish payload data from padding at the end of the transfer.
 *
 * @return TODO
 */
esp_err_t xmodem_recv_buffer(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, void *buf, size_t len, size_t *recv_len);

#ifdef __cplusplus
}
#endif
