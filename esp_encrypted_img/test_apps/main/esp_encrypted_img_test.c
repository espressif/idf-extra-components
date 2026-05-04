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

    /* Warm up AES-GCM with a large buffer to trigger one-time DMA/interrupt
     * allocations that occur when psa_aead_update processes large inputs */
    const uint8_t gcm_key[32] = {0};
    const uint8_t gcm_nonce[12] = {0};
    psa_key_attributes_t gcm_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&gcm_attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&gcm_attr, 256);
    psa_set_key_usage_flags(&gcm_attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&gcm_attr, PSA_ALG_GCM);
    psa_key_id_t gcm_key_id;
    status = psa_import_key(&gcm_attr, gcm_key, sizeof(gcm_key), &gcm_key_id);
    if (status == PSA_SUCCESS) {
        psa_aead_operation_t aead_op = psa_aead_operation_init();
        status = psa_aead_encrypt_setup(&aead_op, gcm_key_id, PSA_ALG_GCM);
        if (status == PSA_SUCCESS) {
            psa_aead_set_nonce(&aead_op, gcm_nonce, sizeof(gcm_nonce));
            /* Use a large stack buffer to trigger DMA-mode AES-GCM */
            uint8_t *gcm_buf = calloc(1, 4096);
            if (gcm_buf) {
                size_t gcm_olen;
                psa_aead_update(&aead_op, gcm_buf, 4096,
                                gcm_buf, 4096, &gcm_olen);
                free(gcm_buf);
            }
        }
        psa_aead_abort(&aead_op);
        psa_destroy_key(gcm_key_id);
    }
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
