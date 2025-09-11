/*
 * SPDX-FileCopyrightText: 2025-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *esp_ds_context_t;
typedef int hmac_key_id_t;
typedef int esp_err_t;

#define ESP_DS_IV_BIT_LEN 128
#define ESP_DS_SIGNATURE_MAX_BIT_LEN 3072
#define ESP_DS_SIGNATURE_MD_BIT_LEN 256
#define ESP_DS_SIGNATURE_M_PRIME_BIT_LEN 32
#define ESP_DS_SIGNATURE_L_BIT_LEN 32
#define ESP_DS_SIGNATURE_PADDING_BIT_LEN 64

#define ESP_DS_C_LEN (((ESP_DS_SIGNATURE_MAX_BIT_LEN * 3 \
        + ESP_DS_SIGNATURE_MD_BIT_LEN   \
        + ESP_DS_SIGNATURE_M_PRIME_BIT_LEN   \
        + ESP_DS_SIGNATURE_L_BIT_LEN   \
        + ESP_DS_SIGNATURE_PADDING_BIT_LEN) / 8))

typedef enum {
    ESP_DS_RSA_1024 = (1024 / 32) - 1,
    ESP_DS_RSA_2048 = (2048 / 32) - 1,
    ESP_DS_RSA_3072 = (3072 / 32) - 1,
    ESP_DS_RSA_4096 = (4096 / 32) - 1
} esp_digital_signature_length_t;

typedef struct esp_digital_signature_data {
    esp_digital_signature_length_t rsa_length;
    uint32_t iv[ESP_DS_IV_BIT_LEN / 32];
    uint8_t c[ESP_DS_C_LEN];
} esp_ds_data_t;

typedef struct esp_ds_data_ctx {
    esp_ds_data_t *esp_ds_data;
    uint8_t efuse_key_id; /* efuse block id in which DS_KEY is stored e.g. 0,1*/
    uint16_t rsa_length_bits; /* length of RSA private key in bits e.g. 2048 */
} esp_ds_data_ctx_t;

#define HMAC_KEY_MAX 5

/**
 * @brief Check if the HMAC key is burnt in efuse.
 *
 * @param hmac_key_id[in] The HMAC key ID to check.
 *
 * @return
 *      - true If the HMAC key is burnt.
 *      - false If the HMAC key is not burnt.
 */
bool esp_encrypted_is_hmac_key_burnt_in_efuse(hmac_key_id_t hmac_key_id);

/**
 * @brief Perform PBKDF2 HMAC-SHA256 key derivation.
 *
 * @param hmac_key_id[in] HMAC key ID.
 * @param salt[in] Pointer to the salt.
 * @param salt_len[in] Length of the salt.
 * @param iteration_count[in] Number of iterations for the key derivation.
 * @param key_length[in] Desired length of the derived key.
 * @param output[out] Buffer to store the derived key.
 *
 * @return
 *     - 0 on success.
 *     - -1 on failure.
 */
int esp_encrypted_img_pbkdf2_hmac_sha256(hmac_key_id_t hmac_key_id, const unsigned char *salt, size_t salt_len,
        size_t iteration_count, size_t key_length, unsigned char *output);

/**
 * @brief Get the digital signature context for secure certificate.
 *
 * @return esp_ds_data_ctx_t* Pointer to the digital signature context.
 */
esp_ds_data_ctx_t *esp_secure_cert_get_ds_ctx(void);

/**
 * @brief Free the digital signature context.
 *
 * @param ds_ctx Pointer to the digital signature context to free.
 */
void esp_secure_cert_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx);
