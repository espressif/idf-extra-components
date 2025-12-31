/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_encrypted_img.h"
#include "mbedtls/version.h"
#include "sdkconfig.h"

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
#include "psa/crypto.h"
#else
#include "mbedtls/gcm.h"
#endif

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
#include "esp_hmac.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GCM_KEY_SIZE        32
#define CACHE_BUF_SIZE      16

typedef enum {
    ESP_PRE_ENC_IMG_READ_MAGIC,
    ESP_PRE_ENC_IMG_READ_GCM,
    ESP_PRE_ENC_IMG_READ_IV,
    ESP_PRE_ENC_IMG_READ_BINSIZE,
    ESP_PRE_ENC_IMG_READ_AUTH,
    ESP_PRE_ENC_IMG_READ_EXTRA_HEADER,
    ESP_PRE_ENC_DATA_DECODE_STATE,
} esp_encrypted_img_state;

/**
 * @brief Internal handle structure for encrypted image decryption
 */
typedef struct esp_encrypted_img_handle {
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if !defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    char *rsa_pem;
    size_t rsa_len;
#endif
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    hmac_key_id_t hmac_key;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */
    uint32_t binary_file_len;
    uint32_t binary_file_read;
    char gcm_key[GCM_KEY_SIZE];
    char iv[IV_SIZE];
    char auth_tag[AUTH_SIZE];
    esp_encrypted_img_state state;
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_aead_operation_t psa_aead_op;
    psa_key_id_t psa_gcm_key_id;
    bool psa_initialized;
#else
    mbedtls_gcm_context gcm_ctx;
#endif
    size_t cache_buf_len;
    char *cache_buf;
} esp_encrypted_img_t;


typedef struct {
    char magic[MAGIC_SIZE];
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    char enc_gcm[ENC_GCM_KEY_SIZE];
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    unsigned char server_ecc_pub_key[SERVER_ECC_KEY_LEN];
    unsigned char kdf_salt[KDF_SALT_SIZE];
    unsigned char reserved[RESERVED_SIZE];
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */
    char iv[IV_SIZE];
    char bin_size[BIN_SIZE_DATA];
    char auth[AUTH_SIZE];
    char extra_header[RESERVED_HEADER];
} pre_enc_bin_header;
#define HEADER_DATA_SIZE    sizeof(pre_enc_bin_header)

// Magic Byte is created using command: echo -n "esp_encrypted_img" | sha256sum
static uint32_t esp_enc_img_magic = 0x0788b6cf;

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
#define HMAC_OUTPUT_SIZE    32
#define PBKDF2_ITERATIONS   2048
#define HKDF_INFO_SIZE      16
#define DER_ASN1_OVERHEAD 30
#define SECP256R1_COORD_SIZE 32
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

#ifdef __cplusplus
}
#endif
