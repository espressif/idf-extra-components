/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "esp_heap_caps.h"
#include "esp_newlib.h"
#include "soc/soc_caps.h"
#include "unity_test_utils_memory.h"

#include "soc/soc_caps.h"

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
#include "psa/crypto.h"
#else
#include "mbedtls/aes.h"
#endif

void setUp(void)
{
#if SOC_AES_SUPPORTED
    // Execute AES operation to allocate AES interrupt
    // allocation memory which is considered as leak otherwise
    const uint8_t plaintext[16] = {0};
    uint8_t ciphertext[16];
    const uint8_t key[16] = { 0 };
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_status_t status = psa_crypto_init();
    TEST_ASSERT_EQUAL(PSA_SUCCESS, status);
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    psa_key_id_t key_id;
    status = psa_import_key(&attributes, key, sizeof(key), &key_id);
    TEST_ASSERT_EQUAL(PSA_SUCCESS, status);
    size_t output_length;
    status = psa_cipher_encrypt(key_id, PSA_ALG_ECB_NO_PADDING, plaintext, sizeof(plaintext),
                                ciphertext, sizeof(ciphertext), &output_length);
    TEST_ASSERT_EQUAL(PSA_SUCCESS, status);
    psa_destroy_key(key_id);
#else
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, plaintext, ciphertext);
    mbedtls_aes_free(&ctx);
#endif
#endif // SOC_AES_SUPPORTED
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(200);
}

void app_main(void)
{
    printf("Running esp_encrypted_img component tests\n");
    unity_run_menu();
}
