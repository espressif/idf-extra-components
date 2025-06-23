/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_mocks.h"
#include <string.h>

uint8_t dummy_pbkdf2_output[32] = {
    0x83, 0x17, 0x93, 0x66, 0x0d, 0xe4, 0x91, 0x33, 0x66, 0xae, 0x1e, 0x37, 0x9b, 0x2c, 0xeb, 0x43,
    0x17, 0xc8, 0x87, 0x00, 0xcc, 0x07, 0x91, 0xd9, 0x8e, 0x5a, 0x2a, 0x2d, 0x5c, 0x71, 0xaf, 0x66
};

bool esp_encrypted_is_hmac_key_burnt_in_efuse(hmac_key_id_t hmac_key_id)
{
    // Simulate the behavior of checking if the HMAC key is burnt in efuse
    // For this example, we'll assume that the key is always burnt in for key ID 2
    if (hmac_key_id == 2) {
        return true;
    }
    return false;
}

int esp_encrypted_img_pbkdf2_hmac_sha256(hmac_key_id_t hmac_key_id, const unsigned char *salt, size_t salt_len,
        size_t iteration_count, size_t key_length, unsigned char *output)
{
    // Simulate the behavior of PBKDF2 HMAC-SHA256 key derivation
    // For this example, we'll just fill the output with a known pattern
    memcpy(output, dummy_pbkdf2_output, key_length);
    return 0; // Indicate success
}
