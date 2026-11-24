/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * AES key unwrap, RFC 3394. mbedtls in ESP-IDF ships no key wrap module, so the unwrap loop
 * lives here, over PSA single-block ECB. Only unwrapping is needed: a device never wraps a key.
 *
 * A wrapped key is 8 bytes longer than the key it carries. The last step compares the recovered
 * integrity value against the constant A6A6A6A6A6A6A6A6. A mismatch means the key encryption key
 * was wrong, which is the normal answer when a release is addressed to another device.
 */
#include <string.h>
#include "psa/crypto.h"
#include "suit_priv.h"

#define KW_BLOCK        8                  /* RFC 3394 works on 64-bit blocks */
#define KW_AES_BLOCK    16
#define KW_ROUNDS       6                  /* the standard fixes six passes */

static const uint8_t KW_IV[KW_BLOCK] = { 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };

esp_err_t suit_aes_key_unwrap(const uint8_t *kek, size_t kek_len, const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t *out_len)
{
    if (kek == NULL || in == NULL || out == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (in_len < 2 * KW_BLOCK || (in_len % KW_BLOCK) != 0 || in_len - KW_BLOCK > *out_len) {
        return ESP_ERR_INVALID_ARG;
    }
    if (kek_len != 16 && kek_len != 24 && kek_len != 32) {
        return ESP_ERR_INVALID_ARG;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        return ESP_FAIL;
    }

    psa_key_attributes_t attr = psa_key_attributes_init();
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, kek_len * 8);
    psa_key_id_t key = 0;
    if (psa_import_key(&attr, kek, kek_len, &key) != PSA_SUCCESS) {
        return ESP_FAIL;
    }

    const size_t n = in_len / KW_BLOCK - 1;        /* 64-bit blocks of the wrapped key */
    uint8_t a[KW_BLOCK];
    memcpy(a, in, KW_BLOCK);
    memcpy(out, in + KW_BLOCK, in_len - KW_BLOCK);

    esp_err_t err = ESP_OK;
    for (int j = KW_ROUNDS - 1; j >= 0 && err == ESP_OK; j--) {
        for (size_t i = n; i >= 1; i--) {
            uint8_t block[KW_AES_BLOCK];
            uint64_t t = (uint64_t)n * (unsigned)j + i;
            memcpy(block, a, KW_BLOCK);
            for (int k = 0; k < KW_BLOCK; k++) {   /* A ^ t, big endian */
                block[KW_BLOCK - 1 - k] ^= (uint8_t)(t >> (8 * k));
            }
            memcpy(block + KW_BLOCK, out + (i - 1) * KW_BLOCK, KW_BLOCK);
            size_t olen = 0;
            if (psa_cipher_decrypt(key, PSA_ALG_ECB_NO_PADDING, block, sizeof(block),
                                   block, sizeof(block), &olen) != PSA_SUCCESS || olen != sizeof(block)) {
                err = ESP_FAIL;
                break;
            }
            memcpy(a, block, KW_BLOCK);
            memcpy(out + (i - 1) * KW_BLOCK, block + KW_BLOCK, KW_BLOCK);
        }
    }
    psa_destroy_key(key);

    if (err == ESP_OK) {
        uint8_t diff = 0;
        for (int i = 0; i < KW_BLOCK; i++) {       /* constant time: a wrong key leaks no timing */
            diff |= (uint8_t)(a[i] ^ KW_IV[i]);
        }
        if (diff != 0) {
            err = ESP_ERR_SUIT_OTA_DECRYPT;
        } else {
            *out_len = in_len - KW_BLOCK;
        }
    }
    memset(a, 0, sizeof(a));
    if (err != ESP_OK) {
        memset(out, 0, in_len - KW_BLOCK);
    }
    return err;
}
