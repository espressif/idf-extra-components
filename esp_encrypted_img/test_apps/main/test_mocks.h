/*
 * SPDX-FileCopyrightText: 2025-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef int hmac_key_id_t;

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
