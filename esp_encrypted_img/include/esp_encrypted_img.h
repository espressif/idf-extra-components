/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include <esp_idf_version.h>

#if 0   //High level layout for state machine

// *INDENT-OFF*
@startuml
[*] --> READ_MAGIC
READ_MAGIC --> READ_MAGIC : READ LEN < 4
READ_MAGIC --> DECODE_MAGIC : READ LEN = 4

DECODE_MAGIC --> READ_GCM : MAGIC VERIFIED
DECODE_MAGIC --> ESP_FAIL : MAGIC VERIFICATION FAILED
PROCESS_BINARY --> ESP_FAIL : DECRYPTION FAILED

READ_GCM --> READ_GCM : READ_LEN < 384
READ_GCM --> DECRYPT_GCM : READ_LEN = 384
DECRYPT_GCM --> ESP_FAIL : DECRYPTION FAILED
DECRYPT_GCM --> READ_IV : DECRYPTION SUCCESSFUL
READ_IV --> READ_IV : READ LEN < 16
READ_IV --> READ_BIN_SIZE
READ_BIN_SIZE --> READ_BIN_SIZE : READ LEN < 5
READ_BIN_SIZE --> READ_AUTH
READ_AUTH --> READ_AUTH : READ LEN < 16
READ_AUTH --> PROCESS_BINARY
PROCESS_BINARY --> PROCESS_BINARY : READ LEN < BIN_SIZE

PROCESS_BINARY --> ESP_OK : READ LEN = BIN_SIZE
ESP_OK --> [*]
ESP_FAIL --> [*]
@enduml
// *INDENT-OFF*
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0))
#define DEPRECATED_ATTRIBUTE __attribute__((deprecated))
#else
#define DEPRECATED_ATTRIBUTE
#endif

typedef void *esp_decrypt_handle_t;

typedef struct {
    union {
        const char *rsa_priv_key;                       /*!< 3072 bit RSA private key in PEM format */
        const char *rsa_pub_key DEPRECATED_ATTRIBUTE;   /*!< This name is kept for backward compatibility purpose,
                                                             but it is not accurate (meaning wise) and hence it would
                                                             be removed in the next major release */
    };
    union {
        size_t rsa_priv_key_len;                        /*!< Length of the buffer pointed to by rsa_priv_key */
        size_t rsa_pub_key_len DEPRECATED_ATTRIBUTE;    /*!< This name is kept for backward compatibility purpose,
                                                             but it is not accurate (meaning wise) and hence it would
                                                             be removed in the next major release */
    };
} esp_decrypt_cfg_t;

#undef DEPRECATED_ATTRIBUTE

typedef struct {
    const char *data_in;    /*!< Pointer to data to be decrypted */
    size_t data_in_len;     /*!< Input data length */
    char *data_out;         /*!< Pointer to decrypted data */
    size_t data_out_len;    /*!< Output data length */
} pre_enc_decrypt_arg_t;

/**
* @brief  This function returns esp_decrypt_handle_t handle.
*
* @param[in]   cfg   pointer to esp_decrypt_cfg_t structure
*
* @return
*    - NULL    On failure
*    - esp_decrypt_handle_t handle
*/
esp_decrypt_handle_t esp_encrypted_img_decrypt_start(const esp_decrypt_cfg_t *cfg);

/**
* @brief  This function performs decryption on input data.
*
* This function must be called only if esp_encrypted_img_decrypt_start() returns successfully.
* This function must be called in a loop since input data might not contain whole binary at once.
* This function must be called till it return ESP_OK.
*
* @note args->data_out must be freed after use provided args->data_out_len is greater than 0
*
* @param[in]        ctx                 esp_decrypt_handle_t handle
* @param[in/out]    args                pointer to pre_enc_decrypt_arg_t
*
* @return
*    - ESP_FAIL                         On failure
*    - ESP_ERR_INVALID_ARG              Invalid arguments
*    - ESP_ERR_NOT_FINISHED             Decryption is in process
*    - ESP_OK                           Success
*/
esp_err_t esp_encrypted_img_decrypt_data(esp_decrypt_handle_t ctx, pre_enc_decrypt_arg_t *args);

/**
* @brief  Clean-up decryption process.
*
* @param[in]   ctx   esp_decrypt_handle_t handle
*
* @note This API cleans the decrypt handle and return ESP_FAIL if the complete data has not been decrypted. Verify if complete data
*       has been decrypted using API `esp_encrypted_img_is_complete_data_received` to prevent an early call to this API.
*
* @return
*    - ESP_FAIL                 On failure
*    - ESP_ERR_INVALID_ARG      Invalid argument
*    - ESP_OK                   Success
*/
esp_err_t esp_encrypted_img_decrypt_end(esp_decrypt_handle_t ctx);

/**
* @brief  Checks if the complete data has been decrypted.
*
* @note This API checks if complete data has been supplied to `esp_encrypted_img_decrypt_data`. This can be used to prevent an early
*       call to `esp_encrypted_img_decrypt_end` which cleans up the decrypt handle. If this API returns true, then call `esp_encrypted_img_decrypt_end`.
*       If this API returns false, and there is some other error (like network error) due to which decryption process should be terminated,
*       call `esp_encrypted_img_decrypt_abort` to clean up the handle.
*
* @param[in]  ctx   esp_decrypt_handle_t handle
*
* @return
*     - true
*     - false
*/
bool esp_encrypted_img_is_complete_data_received(esp_decrypt_handle_t ctx);

/**
* @brief  Abort the decryption process
*
* @param[in]   ctx   esp_decrypt_handle_t handle
*
* @return
*    - ESP_ERR_INVALID_ARG  Invalid argument
*    - ESP_OK               Success
*/
esp_err_t esp_encrypted_img_decrypt_abort(esp_decrypt_handle_t ctx);

/**
* @brief  Get the size of pre encrypted binary image header (`struct pre_enc_bin_header`). The initial header in
*         the image contains magic, credentials (symmetric key) and few other parameters. This API could be useful
*         for scenarios where the entire decrypted image length must be computed by the application including the
*         image header.
*
* @return
*    - Header size of pre encrypted image
*/
uint16_t esp_encrypted_img_get_header_size(void);

#ifdef __cplusplus
}
#endif
