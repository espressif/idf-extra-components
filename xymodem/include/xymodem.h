/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/stat.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_XYMODEM_BASE           0x1E000
#define ESP_ERR_XYMODEM_CANCEL_BY_PEER (ESP_ERR_XYMODEM_BASE + 1)
#define ESP_ERR_XYMODEM_RETRY_EXCEEDED (ESP_ERR_XYMODEM_BASE + 2)
#define ESP_ERR_XYMODEM_BAD_PACKET     (ESP_ERR_XYMODEM_BASE + 3)
#define ESP_ERR_XYMODEM_NO_RESPONSE    (ESP_ERR_XYMODEM_BASE + 4)
#define ESP_ERR_XYMODEM_OUT_OF_SYNC    (ESP_ERR_XYMODEM_BASE + 5)

/**
 * @brief The abstraction of the low-level communication channel used by X/YMODEM protocol layer.
 */
typedef struct {
    /**
     * @brief Receive data through some interface, such as a serial port.
     *
     * @param[in] ctx User-defined context.
     * @param[out] buf Buffer to store received data.
     * @param len Requested length of data to receive.
     * @param[out] recv_len Actual length of data received.
     * @param timeout_ms Timeout in milliseconds. 0 means non-blocking.
     *
     * @return
     *   - ESP_OK if the requested amount of data is received in full, or if the timeout expires.
     *   - others if an error occurs during the transfer. Timeout is NOT considered as an error.
     *
     * @note This function attempts to receive the full requested amount within the timeout period.
     *       If the timeout is reached, return whatever data has been received. Returning early with
     *       a short read before the timeout is not allowed.
     */
    esp_err_t (*recv)(void *ctx, void *buf, size_t len, size_t *recv_len, uint32_t timeout_ms);
    /**
     * @brief Send data through some interface, such as a serial port.
     *
     * @param[in] ctx User-defined context.
     * @param[in] buf Buffer to send.
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
 * @param[in] ctx User-defined context.
 * @param[out] buf Buffer to store read data.
 * @param len Requested length of data to read.
 * @param[out] read_len Actual length of data read.
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
 * @param[in] ctx User-defined context.
 * @param[in] buf Buffer containing the data to write.
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
    /**
     * @note Note that the XMODEM protocol cannot distinguish the payload from trailing padding.
     *       The padding is also delivered through this function.
     */
    xymodem_write_fn_t write;
    void *ctx;
} xmodem_data_sink_t;

#if 0 // YMODEM is not implemented yet
/**
 * @brief File metadata as defined by the YMODEM protocol.
 */
typedef struct {
    const char *filename;
    struct {
        uint32_t file_size : 1;
        uint32_t mtime : 1;
        uint32_t mode : 1;
    } has;
    size_t file_size;
    time_t mtime;
    mode_t mode;
} ymodem_file_info_t;

/**
 * @brief The abstraction representing the source of files to be sent via YMODEM.
 */
typedef struct {
    /**
     * @brief Provide the protocol layer with the metadata of the next file to be sent.
     *
     * @param[in] ctx User-defined context.
     * @param[out] file_info Metadata of the next file to be sent. Set `filename` to NULL or empty string to indicate no more files to send.
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
     * @brief Called by the protocol layer to notify the upper-layer application that the transfer of the file provided by the most recent `next()` call has completed.
     *        The user may release associated resources in this callback, such as closing the file.
     *
     * @param[in] ctx User-defined context.
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
     * @param[in] ctx User-defined context.
     * @param[in] file_info Metadata of the incoming file. The `filename` pointer is only guaranteed to be valid for the duration of this callback.
     *
     * @return ESP_OK if the upper-layer application accepts the information and successfully performs any necessary operations.
     *         Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*next)(void *ctx, const ymodem_file_info_t *file_info);
    /**
     * @brief Write data to the file specified by the most recent `next()` call.
     */
    xymodem_write_fn_t write;
    /**
     * @brief Called by the protocol layer to notify the upper-layer application that the transfer of the file specified by the most recent `next()` call has completed.
     *        The user may release associated resources in this callback, such as closing the file.
     *
     * @param[in] ctx User-defined context.
     * @param result Result of the transfer.
     *
     * @return ESP_OK if the upper-layer application acknowledges the information and successfully performs any necessary operations.
     *         Returning an error will abort the YMODEM transfer.
     */
    esp_err_t (*end)(void *ctx, esp_err_t result);
    void *ctx;
} ymodem_file_sink_t;
#endif

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
    uint32_t timeout_ms;
    uint32_t max_retry_cnt;
    xymodem_block_size_t block_size;
} xymodem_send_config_t;

typedef struct {
    uint32_t timeout_ms;
    uint32_t max_retry_cnt;
    xymodem_check_type_t check_type;
} xymodem_recv_config_t;

/**************************************
 * Generic APIs
 **************************************/

/**
 * @brief Send data using the XMODEM protocol.
 *
 * This function performs a complete XMODEM transmission using the provided
 * transport and data source, and blocks until the transmission completes,
 * fails, or is cancelled.
 *
 * @param transport Transport interface used for sending and receiving protocol data.
 * @param config XMODEM sender configuration.
 * @param source Data source providing the payload to be transmitted.
 *
 * @return
 *   - ESP_OK: Transmission completed successfully.
 *   - ESP_ERR_INVALID_ARG: One or more arguments or configuration values are invalid.
 *   - ESP_ERR_NO_MEM: Not enough memory to complete the transmission.
 *   - ESP_ERR_XYMODEM_NO_RESPONSE: The peer did not respond within the configured timeout.
 *   - ESP_ERR_XYMODEM_CANCEL_BY_PEER: The transmission was cancelled by the peer.
 *   - ESP_ERR_XYMODEM_RETRY_EXCEEDED: The maximum number of retries was exceeded.
 *   - Other errors returned by the transport or data source.
 */
esp_err_t xmodem_send(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const xmodem_data_source_t *source);

/**
 * @brief Receive data using the XMODEM protocol.
 *
 * This function performs a complete XMODEM reception using the provided
 * transport and data sink, and blocks until the transmission completes,
 * fails, or is cancelled.
 *
 * @param transport Transport interface used for sending and receiving protocol data.
 * @param config XMODEM receiver configuration.
 * @param sink Data sink that receives the transmitted payload.
 *
 * @return
 *   - ESP_OK: Reception completed successfully.
 *   - ESP_ERR_INVALID_ARG: One or more arguments or configuration values are invalid.
 *   - ESP_ERR_NO_MEM: Not enough memory to complete the reception.
 *   - ESP_ERR_XYMODEM_NO_RESPONSE: The peer did not respond within the configured timeout.
 *   - ESP_ERR_XYMODEM_CANCEL_BY_PEER: The transmission was cancelled by the peer.
 *   - ESP_ERR_XYMODEM_RETRY_EXCEEDED: The maximum number of retries was exceeded.
 *   - ESP_ERR_XYMODEM_OUT_OF_SYNC: The received packet sequence is out of sync.
 *   - Other errors returned by the transport or data sink.
 */
esp_err_t xmodem_recv(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, const xmodem_data_sink_t *sink);

#if 0  // YMODEM is not implemented yet
/**
 * @brief Send files using the YMODEM protocol. Not implemented yet.
 */
esp_err_t ymodem_send(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const ymodem_file_source_t *source);

/**
 * @brief Receive files using the YMODEM protocol. Not implemented yet.
 */
esp_err_t ymodem_recv(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, const ymodem_file_sink_t *sink);
#endif

/**************************************
 * Convenience APIs
 * implemented on top of the generic APIs
 **************************************/

/**
 * @brief Directly send the contents of a buffer using XMODEM.
 *
 * @param[in] transport The transport layer to use.
 * @param[in] config The send configuration.
 * @param[in] buf The buffer to send.
 * @param len The length of the buffer.
 *
 * @return
 *   - ESP_OK if the data was successfully sent.
 *   - others if an error occurs (same as `xmodem_send()`).
 */
esp_err_t xmodem_send_buffer(const xymodem_transport_t *transport, const xymodem_send_config_t *config, const void *buf, size_t len);

/**
 * @brief Directly receive data into a buffer using XMODEM.
 *
 * @param[in] transport The transport layer to use.
 * @param[in] config The receive configuration.
 * @param[out] buf The buffer to receive the data.
 * @param len The length of the buffer.
 * @param[out] recv_len Number of bytes stored in `buf`. May be NULL.
 *
 * @note XMODEM cannot distinguish payload data from trailing padding, so the last packet may be larger
 *       than the remaining buffer space. Bytes from that packet that fit are stored; the rest of the
 *       packet is discarded. If another packet arrives after `buf` is already full, the transfer is aborted.
 *
 * @return
 *   - ESP_OK if the transfer completed. `recv_len` (when not NULL) is the number of bytes stored in `buf`, which is at most `len`.
 *   - ESP_ERR_NO_MEM: `buf` was filled and the peer sent another packet.
 *   - others if an error occurs (same as `xmodem_recv()`). `recv_len` still reports how many bytes were stored before the failure.
 */
esp_err_t xmodem_recv_buffer(const xymodem_transport_t *transport, const xymodem_recv_config_t *config, void *buf, size_t len, size_t *recv_len);

#ifdef __cplusplus
}
#endif
