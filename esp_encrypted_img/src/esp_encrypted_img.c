/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include <esp_log.h>
#include <esp_err.h>
#include "sys/param.h"

#include "esp_encrypted_img.h"
#include "esp_encrypted_img_priv.h"

#include "mbedtls/version.h"
#include "mbedtls/pk.h"
#include "sdkconfig.h"

#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/pkcs5.h"
#endif
#include "esp_random.h"
#include "esp_encrypted_img_utilities.h"

#if SOC_HMAC_SUPPORTED
#include "esp_efuse.h"
#include "esp_efuse_chip.h"
#endif /* SOC_HMAC_SUPPORTED */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
#if __has_include("psa_crypto_driver_esp_rsa_ds.h")
#include "psa_crypto_driver_esp_rsa_ds.h"
#endif /* __has_include("psa_crypto_driver_esp_rsa_ds.h") */
#else
#if __has_include("rsa_dec_alt.h")
#include "rsa_dec_alt.h"
#else
#error "DS Peripheral is not supported on this version of ESP-IDF"
#endif /* __has_include("rsa_dec_alt.h") */
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */

#include "esp_random.h"

static const char *TAG = "esp_encrypted_img";

/*
 * GCM Abstraction Layer Implementations
 */
static esp_err_t gcm_init_and_set_key(esp_encrypted_img_t *handle, const unsigned char *key, size_t key_bits)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key_bits);

    status = psa_import_key(&attributes, key, key_bits / 8, &handle->psa_gcm_key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key for GCM failed: %d", (int)status);
        psa_reset_key_attributes(&attributes);
        return ESP_FAIL;
    }

    handle->psa_aead_op = psa_aead_operation_init();
    status = psa_aead_decrypt_setup(&handle->psa_aead_op, handle->psa_gcm_key_id, PSA_ALG_GCM);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_aead_decrypt_setup failed: %d", (int)status);
        psa_aead_abort(&handle->psa_aead_op);
        psa_destroy_key(handle->psa_gcm_key_id);
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    mbedtls_gcm_init(&handle->gcm_ctx);
    int ret = mbedtls_gcm_setkey(&handle->gcm_ctx, MBEDTLS_CIPHER_ID_AES, key, key_bits);
    if (ret != 0) {
        ESP_LOGE(TAG, "Error: mbedtls_gcm_set_key: -0x%04x", (unsigned int) - ret);
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

static esp_err_t gcm_start(esp_encrypted_img_t *handle, const unsigned char *iv, size_t iv_len)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_status_t status = psa_aead_set_nonce(&handle->psa_aead_op, iv, iv_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_aead_set_nonce failed: %d", (int)status);
        psa_aead_abort(&handle->psa_aead_op);
        psa_destroy_key(handle->psa_gcm_key_id);
        return ESP_FAIL;
    }
    handle->psa_initialized = true;
    return ESP_OK;
#else
    int ret;
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    ret = mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, iv, iv_len, NULL, 0);
#else
    ret = mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, iv, iv_len);
#endif
    if (ret != 0) {
        ESP_LOGE(TAG, "Error: mbedtls_gcm_starts: -0x%04x", (unsigned int) - ret);
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

static esp_err_t gcm_update(esp_encrypted_img_t *handle, const unsigned char *input, size_t input_len,
                            unsigned char *output, size_t output_size, size_t *output_len)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_status_t status = psa_aead_update(&handle->psa_aead_op, input, input_len,
                                          output, output_size, output_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_aead_update failed: %d", (int)status);
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    int ret;
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    ret = mbedtls_gcm_update(&handle->gcm_ctx, input_len, input, output);
    if (output_len) {
        *output_len = input_len;
    }
#else
    size_t olen;
    ret = mbedtls_gcm_update(&handle->gcm_ctx, input, input_len, output, output_size, &olen);
    if (output_len) {
        *output_len = olen;
    }
#endif
    if (ret != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

static esp_err_t gcm_finish_and_verify(esp_encrypted_img_t *handle, const unsigned char *tag, size_t tag_len)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    size_t output_len = 0;
    psa_status_t status = psa_aead_verify(&handle->psa_aead_op, NULL, 0, &output_len, tag, tag_len);
    if (status != PSA_SUCCESS) {
        if (status == PSA_ERROR_INVALID_SIGNATURE) {
            ESP_LOGE(TAG, "Invalid Auth Tag");
        } else {
            ESP_LOGE(TAG, "psa_aead_verify failed: %d", (int)status);
        }
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    unsigned char got_auth[AUTH_SIZE] = {0};
    int ret;
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    ret = mbedtls_gcm_finish(&handle->gcm_ctx, got_auth, tag_len);
#else
    size_t olen;
    ret = mbedtls_gcm_finish(&handle->gcm_ctx, NULL, 0, &olen, got_auth, tag_len);
#endif
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_gcm_finish failed: %d", ret);
        return ESP_FAIL;
    }
    if (memcmp(got_auth, tag, tag_len) != 0) {
        ESP_LOGE(TAG, "Invalid Auth");
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

static void gcm_cleanup(esp_encrypted_img_t *handle)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    if (handle->psa_initialized) {
        psa_aead_abort(&handle->psa_aead_op);
        psa_destroy_key(handle->psa_gcm_key_id);
        handle->psa_initialized = false;
    }
#else
    mbedtls_gcm_free(&handle->gcm_ctx);
#endif
}

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#define RSA_MPI_ASN1_HEADER_SIZE 11

#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
int mbedtls_esp_random(void *ctx, unsigned char *buf, size_t len)
{
    (void) ctx;
    esp_fill_random(buf, len);
    return 0;
}
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

static int decipher_gcm_key(const char *enc_gcm, esp_encrypted_img_t *handle)
{
#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    if (handle == NULL || handle->ds_data == NULL) {
        ESP_LOGE(TAG, "Invalid argument: handle or ds_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    psa_key_id_t rsa_key_id = PSA_KEY_ID_NULL;
    psa_status_t status;
    psa_algorithm_t alg = PSA_ALG_RSA_PKCS1V15_CRYPT;

    esp_ds_data_ctx_t *ds_key = (esp_ds_data_ctx_t *)handle->ds_data;

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
    psa_set_key_bits(&attributes, ds_key->rsa_length_bits);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_ESP_RSA_DS);
    status = psa_import_key(&attributes,
                            (const uint8_t *)ds_key,
                            sizeof(*ds_key),
                            &rsa_key_id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
        return ESP_FAIL;
    }
    size_t olen = 0;
    status = psa_asymmetric_decrypt(rsa_key_id, alg, (const unsigned char *)enc_gcm,
                                    ENC_GCM_KEY_SIZE, NULL, 0,
                                    (unsigned char *)handle->gcm_key,
                                    GCM_KEY_SIZE,
                                    &olen);
    psa_destroy_key(rsa_key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_asymmetric_decrypt failed: %d", (int)status);
        return ESP_FAIL;
    }
#else
    int ret;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    ret = mbedtls_pk_setup_rsa_alt(&pk, NULL, esp_ds_rsa_decrypt, NULL, esp_ds_get_keylen);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_setup_rsa_alt returned -0x%04x\n", (unsigned int) - ret);
        mbedtls_pk_free(&pk);
        return ret;
    }

    size_t olen = 0;
    ret = mbedtls_pk_decrypt(&pk, (const unsigned char *)enc_gcm, ENC_GCM_KEY_SIZE,
                             (unsigned char *)handle->gcm_key, &olen, GCM_KEY_SIZE,
                             mbedtls_esp_random, NULL);
    mbedtls_pk_free(&pk);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_decrypt returned -0x%04x\n", (unsigned int) - ret);
        return ret;
    }
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

    void *tmp_buf = realloc(handle->cache_buf, CACHE_BUF_SIZE);
    if (!tmp_buf) {
        ESP_LOGE(TAG, "Failed to reallocate memory for cache buffer");
        return ESP_ERR_NO_MEM;
    }
    handle->cache_buf = tmp_buf;
    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;
    return ESP_OK;
}

#else

static int decipher_gcm_key(const char *enc_gcm, esp_encrypted_img_t *handle)
{
    int ret = 1;
    size_t olen = 0;

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_key_id_t rsa_key_id = PSA_KEY_ID_NULL;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;
    mbedtls_pk_context *pk = calloc(1, sizeof(mbedtls_pk_context));
    if (pk == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for mbedtls_pk_context");
        return ESP_ERR_NO_MEM;
    }
    unsigned char *key_buf = NULL;
    size_t key_buf_size = MBEDTLS_MPI_MAX_SIZE * 2;

    ESP_LOGI(TAG, "Reading RSA private key (PSA)");

    mbedtls_pk_init(pk);

    /* Parse RSA key from PEM using mbedtls */
    ret = mbedtls_pk_parse_key(pk, (const unsigned char *)handle->rsa_pem, handle->rsa_len, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_parse_key returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    /* Export to DER format for PSA import */
    key_buf = calloc(1, key_buf_size);
    if (key_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for key buffer");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }

    ret = mbedtls_pk_write_key_der(pk, key_buf, key_buf_size);
    if (ret < 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_write_key_der returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    /* DER is written at the end of the buffer */
    size_t key_len = ret;
    unsigned char *key_start = key_buf + key_buf_size - key_len;

    /* Import RSA key to PSA */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_RSA_PKCS1V15_CRYPT);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);

    status = psa_import_key(&attributes, key_start, key_len, &rsa_key_id);
    mbedtls_pk_free(pk);
    free(pk);
    pk = NULL;

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
        ret = ESP_FAIL;
        goto exit;
    }

    /* Perform RSA PKCS#1 v1.5 decryption */
    status = psa_asymmetric_decrypt(rsa_key_id,
                                    PSA_ALG_RSA_PKCS1V15_CRYPT,
                                    (const unsigned char *)enc_gcm,
                                    ENC_GCM_KEY_SIZE,
                                    NULL, 0,  /* No salt */
                                    (unsigned char *)handle->gcm_key,
                                    GCM_KEY_SIZE,
                                    &olen);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_asymmetric_decrypt failed: %d", (int)status);
        ret = ESP_FAIL;
        goto exit;
    }

    ret = 0;

    void *tmp_buf = realloc(handle->cache_buf, CACHE_BUF_SIZE);
    if (!tmp_buf) {
        ESP_LOGE(TAG, "Failed to reallocate memory for cache buffer");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    handle->cache_buf = tmp_buf;
    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;

exit:
    if (pk) {
        mbedtls_pk_free(pk);
        free(pk);
    }
    if (rsa_key_id != PSA_KEY_ID_NULL) {
        psa_destroy_key(rsa_key_id);
    }
    if (key_buf) {
        mbedtls_platform_zeroize(key_buf, key_buf_size);
        free(key_buf);
    }
    if (handle->rsa_pem) {
        mbedtls_platform_zeroize(handle->rsa_pem, handle->rsa_len);
        free(handle->rsa_pem);
        handle->rsa_pem = NULL;
    }
    return ret;

#else /* !CONFIG_MBEDTLS_VER_4_X_SUPPORT */
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mbedtls_pk_encrypt";

    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_entropy_init( &entropy );
    mbedtls_pk_init( &pk );

    if ((ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func,
                                      &entropy, (const unsigned char *) pers,
                                      strlen(pers))) != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ctr_drbg_seed returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    ESP_LOGI(TAG, "Reading RSA private key");

#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) handle->rsa_pem, handle->rsa_len, NULL, 0)) != 0) {
#else
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) handle->rsa_pem, handle->rsa_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
#endif
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_parse_keyfile returned -0x%04x\n", (unsigned int) - ret );
        goto exit;
    }

    if (( ret = mbedtls_pk_decrypt( &pk, (const unsigned char *)enc_gcm, ENC_GCM_KEY_SIZE, (unsigned char *)handle->gcm_key, &olen, GCM_KEY_SIZE,
                                    mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 ) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_decrypt returned -0x%04x\n", (unsigned int) - ret );
        goto exit;
    }
    void *tmp_buf = realloc(handle->cache_buf, CACHE_BUF_SIZE);
    if (!tmp_buf) {
        ESP_LOGE(TAG, "Failed to reallocate memory for cache buffer");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    handle->cache_buf = tmp_buf;
    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;
exit:
    if (handle->rsa_pem) {
        mbedtls_platform_zeroize(handle->rsa_pem, handle->rsa_len);
        free(handle->rsa_pem);
        handle->rsa_pem = NULL;
    }
    mbedtls_pk_free( &pk );
    mbedtls_entropy_free( &entropy );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    return (ret);
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
}

static esp_err_t esp_encrypted_img_export_rsa_pub_key(const char *rsa_pem, size_t rsa_len, uint8_t **pub_key, size_t *pub_key_len)
{
    int ret = 0;
    if (rsa_pem == NULL) {
        ESP_LOGE(TAG, "RSA private key is not set");
        return ESP_ERR_INVALID_ARG;
    }
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    /* For mbedtls 4.x, use PSA-based key parsing */
    ret = mbedtls_pk_parse_key(&pk, (const unsigned char *)rsa_pem, rsa_len, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_parse_key returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    /* Export public key in DER SubjectPublicKeyInfo format first */
    size_t max_pub_key_size = MBEDTLS_MPI_MAX_SIZE * 2;
    unsigned char *der_buf = calloc(1, max_pub_key_size);
    if (der_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DER buffer");
        goto exit;
    }

    ret = mbedtls_pk_write_pubkey_der(&pk, der_buf, max_pub_key_size);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write public key DER: -0x%04x", (unsigned int) - ret);
        free(der_buf);
        goto exit;
    }

    /* DER is written at the end of the buffer */
    size_t der_len = ret;
    unsigned char *der_start = der_buf + max_pub_key_size - der_len;

    /* Parse SubjectPublicKeyInfo to extract raw public key (BIT STRING content)
     * Structure: SEQUENCE { SEQUENCE { OID, params }, BIT STRING { raw_key } }
     * We need to skip the outer SEQUENCE and algorithm SEQUENCE to get the BIT STRING */
    unsigned char *p = der_start;
    size_t len;

    /* Skip outer SEQUENCE tag and length */
    if (*p++ != 0x30) { /* SEQUENCE */
        ESP_LOGE(TAG, "Invalid DER: expected SEQUENCE");
        free(der_buf);
        goto exit;
    }
    /* Skip length (may be 1 or more bytes) */
    if (*p & 0x80) {
        size_t len_bytes = *p++ & 0x7f;
        p += len_bytes;
    } else {
        p++;
    }

    /* Skip algorithm SEQUENCE */
    if (*p++ != 0x30) { /* SEQUENCE */
        ESP_LOGE(TAG, "Invalid DER: expected algorithm SEQUENCE");
        free(der_buf);
        goto exit;
    }
    /* Get algorithm sequence length and skip it */
    if (*p & 0x80) {
        size_t len_bytes = *p++ & 0x7f;
        len = 0;
        for (size_t i = 0; i < len_bytes; i++) {
            len = (len << 8) | *p++;
        }
    } else {
        len = *p++;
    }
    p += len; /* Skip algorithm sequence content */

    /* Now at BIT STRING */
    if (*p++ != 0x03) { /* BIT STRING */
        ESP_LOGE(TAG, "Invalid DER: expected BIT STRING");
        free(der_buf);
        goto exit;
    }
    /* Get BIT STRING length */
    if (*p & 0x80) {
        size_t len_bytes = *p++ & 0x7f;
        len = 0;
        for (size_t i = 0; i < len_bytes; i++) {
            len = (len << 8) | *p++;
        }
    } else {
        len = *p++;
    }
    /* Skip unused bits byte (should be 0x00) */
    p++;
    len--;

    /* p now points to raw public key, len is its length */
    *pub_key = calloc(1, len);
    if (*pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for public key");
        free(der_buf);
        goto exit;
    }
    memcpy(*pub_key, p, len);
    *pub_key_len = len;

    free(der_buf);
    mbedtls_pk_free(&pk);
    return ESP_OK;

exit:
    if (*pub_key) {
        free(*pub_key);
        *pub_key = NULL;
        *pub_key_len = 0;
    }
    mbedtls_pk_free(&pk);
    return ESP_FAIL;

#else /* !CONFIG_MBEDTLS_VER_4_X_SUPPORT */
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_entropy_init( &entropy );

#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) rsa_pem, rsa_len, NULL, 0)) != 0) {
#else
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) rsa_pem, rsa_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
#endif
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_parse_key returned -0x%04x\n", (unsigned int) - ret );
        goto exit;
    }

    if (mbedtls_pk_get_type(&pk) != MBEDTLS_PK_RSA) {
        ESP_LOGE(TAG, "Public key is not RSA");
        goto exit;
    }

    const mbedtls_rsa_context *rsa_ctx = mbedtls_pk_rsa(pk);
    if (rsa_ctx == NULL) {
        ESP_LOGE(TAG, "Failed to get RSA context from public key");
        goto exit;
    }

    size_t max_pub_key_size = MBEDTLS_MPI_MAX_SIZE + RSA_MPI_ASN1_HEADER_SIZE;
    *pub_key = calloc(1, max_pub_key_size + 1);
    if (*pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for public key");
        goto exit;
    }

    unsigned char *c = *pub_key + max_pub_key_size;

    ret = mbedtls_pk_write_pubkey(&c, *pub_key, &pk);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write public key: -0x%04x", (unsigned int) - ret);
        goto exit;
    }
    if (c - *pub_key < 0) {
        ESP_LOGE(TAG, "Public key buffer is too small");
        goto exit;
    }
    // Adjust the length of the public key
    *pub_key_len = ret;
    // Move the memory to the start of the buffer
    // This is necessary because mbedtls_pk_write_pubkey writes the key in reverse order
    // and we need to adjust the pointer to point to the start of the key.
    memmove(*pub_key, c, *pub_key_len);
    // Resize the public key buffer to the actual length
    unsigned char *temp_pub_key = realloc(*pub_key, *pub_key_len);
    if (temp_pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to resize public key buffer");
        goto exit;
    }
    *pub_key = temp_pub_key;

    // Free the resources
    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    return ESP_OK;
exit:
    if (*pub_key) {
        free(*pub_key);
        *pub_key = NULL;
        *pub_key_len = 0;
    }
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_pk_free(&pk);
    return ESP_FAIL;
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
}
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)

static uint8_t pbkdf2_salt[32] = {
    0x0e, 0x21, 0x60, 0x64, 0x2d, 0xae, 0x76, 0xd3, 0x34, 0x48, 0xe4, 0x3d, 0x77, 0x20, 0x12, 0x3d,
    0x9f, 0x3b, 0x1e, 0xce, 0xb8, 0x8e, 0x57, 0x3a, 0x4e, 0x8f, 0x7f, 0xb9, 0x4f, 0xf0, 0xc8, 0x69
};

#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
static int mbedtls_esp_random(void *ctx, unsigned char *buf, size_t len)
{
    esp_fill_random(buf, len);
    return 0;
}
#else
static int psa_hkdf_derive(const uint8_t *ikm, size_t ikm_len,
                           const uint8_t *salt, size_t salt_len,
                           const uint8_t *info, size_t info_len,
                           uint8_t *okm, size_t okm_len)
{
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;

    status = psa_key_derivation_setup(&operation, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_setup failed: %d", (int)status);
        return ESP_FAIL;
    }

    /* Input salt */
    status = psa_key_derivation_input_bytes(&operation,
                                            PSA_KEY_DERIVATION_INPUT_SALT,
                                            salt, salt_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_input_bytes (salt) failed: %d", (int)status);
        goto abort;
    }

    /* Input IKM (shared secret) */
    status = psa_key_derivation_input_bytes(&operation,
                                            PSA_KEY_DERIVATION_INPUT_SECRET,
                                            ikm, ikm_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_input_bytes (secret) failed: %d", (int)status);
        goto abort;
    }

    /* Input info */
    status = psa_key_derivation_input_bytes(&operation,
                                            PSA_KEY_DERIVATION_INPUT_INFO,
                                            info, info_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_input_bytes (info) failed: %d", (int)status);
        goto abort;
    }

    /* Output derived key */
    status = psa_key_derivation_output_bytes(&operation, okm, okm_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_output_bytes failed: %d", (int)status);
        goto abort;
    }

    psa_key_derivation_abort(&operation);
    return ESP_OK;

abort:
    psa_key_derivation_abort(&operation);
    return ESP_FAIL;
}
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

static esp_err_t compute_ecc_key_with_hmac(hmac_key_id_t hmac_key, mbedtls_mpi *ecc_priv_key)
{
    esp_err_t err = ESP_OK;
    if (ecc_priv_key == NULL) {
        ESP_LOGE(TAG, "ECC key buffer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int ret = 0;
    uint8_t hmac_output[HMAC_OUTPUT_SIZE] = {0};
    mbedtls_ecp_group grp;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(ecc_priv_key);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    err = esp_encrypted_img_pbkdf2_hmac_sha256(hmac_key, pbkdf2_salt, sizeof(pbkdf2_salt),
            PBKDF2_ITERATIONS, HMAC_OUTPUT_SIZE, hmac_output);
    if (err != 0) {
        ESP_LOGE(TAG, "Failed to calculate ECC key: [0x%02X] (%s)", err, esp_err_to_name(err));
        goto cleanup;
    }

    // Step 2: Convert output to scalar mod curve order
    ret = mbedtls_mpi_read_binary(ecc_priv_key, hmac_output, HMAC_OUTPUT_SIZE);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_mpi_read_binary returned -0x%04x\n", (unsigned int) - ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_check_privkey(&grp, ecc_priv_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecp_check_privkey returned -0x%04x\n", (unsigned int) - ret);
        goto cleanup;
    }

    ESP_LOGI(TAG, "ECC key derived successfully");

cleanup:
    mbedtls_ecp_group_free(&grp);
    return ret;
}

static int derive_ota_ecc_device_key(hmac_key_id_t hmac_key, mbedtls_mpi *ecc_priv_key)
{
    // Although we have checked this during the esp_encrypted_img_decrypt_start() call,
    // we will check again here to ensure that the HMAC key is valid.
    if (!esp_encrypted_is_hmac_key_burnt_in_efuse(hmac_key)) {
        ESP_LOGE(TAG, "Could not find HMAC key in configured eFuse block!");
        return ESP_ERR_ENCRYPTED_IMAGE_HMAC_KEY_NOT_FOUND;
    }

    esp_err_t err = compute_ecc_key_with_hmac(hmac_key, ecc_priv_key);
    return err;
}

static mbedtls_ecp_point *get_server_public_point(const char *data, size_t len)
{
    int ret;
    uint8_t *server_public_key = NULL;
    mbedtls_ecp_point *server_public_point = NULL;
    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);

    if ((ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1)) != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecp_group_load returned -0x%04x\n", (unsigned int) - ret);
        return NULL;
    }

    server_public_point = calloc(1, sizeof(mbedtls_ecp_point));
    if (server_public_point == NULL) {
        ESP_LOGE(TAG, "failed to allocate memory for server public point");
        goto cleanup;
    }
    mbedtls_ecp_point_init(server_public_point);

    server_public_key = calloc(1, len + 1);
    if (server_public_key == NULL) {
        ESP_LOGE(TAG, "failed to allocate memory for server public key");
        mbedtls_ecp_point_free(server_public_point);
        server_public_point = NULL;
        goto cleanup;
    }
    server_public_key[0] = 0x04; // Uncompressed point
    memcpy(server_public_key + 1, data, len);

    ret = mbedtls_ecp_point_read_binary(&grp, server_public_point, (const unsigned char *)server_public_key, len + 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecp_point_read_binary returned -0x%04x\n", (unsigned int) - ret);
        mbedtls_ecp_point_free(server_public_point);
        free(server_public_key);
        server_public_key = NULL;
        return NULL;
    }

    ret = mbedtls_ecp_check_pubkey(&grp, server_public_point);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecp_check_pubkey returned -0x%04x\n", (unsigned int) - ret);
        mbedtls_ecp_point_free(server_public_point);
        free(server_public_key);
        server_public_key = NULL;
        return NULL;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    if (server_public_key) {
        memset(server_public_key, 0, len + 1);
        free(server_public_key);
        server_public_key = NULL;
    }
    return server_public_point;
}

static unsigned char *get_kdf_salt_from_header(const char *data, size_t len)
{
    unsigned char *kdf_salt = NULL;
    if (len >= KDF_SALT_SIZE) {
        kdf_salt = calloc(1, KDF_SALT_SIZE);
        if (kdf_salt == NULL) {
            ESP_LOGE(TAG, "failed to allocate memory for kdf_salt");
            return NULL;
        }
        memcpy(kdf_salt, data, KDF_SALT_SIZE);
    }
    return kdf_salt;
}


static int derive_gcm_key(const char *data, esp_encrypted_img_t *handle)
{
    int ret = 0;
    uint8_t *derived_key = calloc(1, GCM_KEY_SIZE);
    if (derived_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for derived key");
        return ESP_ERR_NO_MEM;
    }
    mbedtls_ecp_point *server_public_point = NULL;
    unsigned char *kdf_salt = NULL;
    uint8_t shared_secret_bytes[32] = {0};

    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);
    if ((ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1)) != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecp_group_load returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    server_public_point = get_server_public_point(data, SERVER_ECC_KEY_LEN);
    if (server_public_point == NULL) {
        ESP_LOGE(TAG, "Failed to get server public point");
        ret = ESP_FAIL;
        goto exit;
    }
    kdf_salt = get_kdf_salt_from_header(data + SERVER_ECC_KEY_LEN, KDF_SALT_SIZE);
    mbedtls_mpi device_private_mpi;

    esp_err_t err = derive_ota_ecc_device_key(handle->hmac_key, &device_private_mpi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to derive ECC device key");
        goto exit;
    }

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    {
        psa_key_id_t device_key_id = PSA_KEY_ID_NULL;
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_status_t status;
        uint8_t priv_key_buf[32];
        uint8_t server_pub_key_buf[65];  /* 0x04 || X || Y */
        size_t shared_secret_len = 0;

        /* Convert device private key MPI to raw bytes */
        ret = mbedtls_mpi_write_binary(&device_private_mpi, priv_key_buf, sizeof(priv_key_buf));
        mbedtls_mpi_free(&device_private_mpi);
        if (ret != 0) {
            ESP_LOGE(TAG, "failed\n  ! mbedtls_mpi_write_binary returned -0x%04x\n", (unsigned int) - ret);
            goto exit;
        }

        /* Import private key to PSA */
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
        psa_set_key_algorithm(&attributes, PSA_ALG_ECDH);
        psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
        psa_set_key_bits(&attributes, 256);

        status = psa_import_key(&attributes, priv_key_buf, sizeof(priv_key_buf), &device_key_id);
        mbedtls_platform_zeroize(priv_key_buf, sizeof(priv_key_buf));

        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
            ret = ESP_FAIL;
            goto exit;
        }

        /* Prepare server public key in uncompressed format */
        server_pub_key_buf[0] = 0x04;  /* Uncompressed point */
        mbedtls_mpi_write_binary(&server_public_point->MBEDTLS_PRIVATE(X),
                                 server_pub_key_buf + 1, 32);
        mbedtls_mpi_write_binary(&server_public_point->MBEDTLS_PRIVATE(Y),
                                 server_pub_key_buf + 33, 32);

        /* Perform ECDH using PSA */
        status = psa_raw_key_agreement(PSA_ALG_ECDH,
                                       device_key_id,
                                       server_pub_key_buf, sizeof(server_pub_key_buf),
                                       shared_secret_bytes, sizeof(shared_secret_bytes),
                                       &shared_secret_len);

        psa_destroy_key(device_key_id);

        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_raw_key_agreement failed: %d", (int)status);
            ret = ESP_FAIL;
            goto exit;
        }

        /* Perform HKDF using PSA */
        ret = psa_hkdf_derive(shared_secret_bytes, sizeof(shared_secret_bytes),
                              kdf_salt, KDF_SALT_SIZE,
                              (const uint8_t *)"_esp_enc_img_ecc", HKDF_INFO_SIZE,
                              derived_key, GCM_KEY_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "psa_hkdf_derive failed");
            goto exit;
        }
    }
#else /* !CONFIG_MBEDTLS_VER_4_X_SUPPORT */
    {
        mbedtls_mpi shared_secret;
        mbedtls_mpi_init(&shared_secret);

        ret = mbedtls_ecdh_compute_shared(&grp, &shared_secret, server_public_point, &device_private_mpi,
                                          mbedtls_esp_random, NULL);
        mbedtls_mpi_free(&device_private_mpi);
        if (ret != 0) {
            ESP_LOGE(TAG, "failed\n  ! mbedtls_ecdh_compute_shared returned -0x%04x\n", (unsigned int) - ret);
            goto exit;
        }

        ret = mbedtls_mpi_write_binary(&shared_secret, shared_secret_bytes, sizeof(shared_secret_bytes));
        mbedtls_mpi_free(&shared_secret);
        if (ret != 0) {
            ESP_LOGE(TAG, "failed\n  ! mbedtls_mpi_write_binary returned -0x%04x\n", (unsigned int) - ret);
            goto exit;
        }

        unsigned char *hkdf_info = calloc(1, HKDF_INFO_SIZE);
        if (hkdf_info == NULL) {
            ESP_LOGE(TAG, "failed to allocate memory for hkdf_info");
            ret = ESP_ERR_NO_MEM;
            goto exit;
        }
        memcpy(hkdf_info, "_esp_enc_img_ecc", HKDF_INFO_SIZE);

        ret = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), kdf_salt, KDF_SALT_SIZE,
                           (const unsigned char *)shared_secret_bytes, sizeof(shared_secret_bytes),
                           hkdf_info, HKDF_INFO_SIZE, derived_key, GCM_KEY_SIZE);
        mbedtls_platform_zeroize(hkdf_info, HKDF_INFO_SIZE);
        free(hkdf_info);
        if (ret != 0) {
            ESP_LOGE(TAG, "failed\n  ! mbedtls_hkdf returned -0x%04x\n", (unsigned int) - ret);
            goto exit;
        }
    }
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

    memcpy(handle->gcm_key, derived_key, GCM_KEY_SIZE);
    ESP_LOGI(TAG, "GCM key derived successfully");

    void *tmp_buf = realloc(handle->cache_buf, CACHE_BUF_SIZE);
    if (!tmp_buf) {
        ESP_LOGE(TAG, "Failed to reallocate memory for cache buffer");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    handle->cache_buf = tmp_buf;

    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;
exit:
    mbedtls_ecp_group_free(&grp);
    if (server_public_point) {
        mbedtls_ecp_point_free(server_public_point);
        free(server_public_point);
    }
    mbedtls_platform_zeroize(shared_secret_bytes, sizeof(shared_secret_bytes));
    mbedtls_platform_zeroize(derived_key, GCM_KEY_SIZE);
    free(derived_key);
    if (kdf_salt) {
        mbedtls_platform_zeroize(kdf_salt, KDF_SALT_SIZE);
        free(kdf_salt);
    }

    return ret;
}

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
static esp_err_t esp_encrypted_img_export_ecies_pub_key(hmac_key_id_t hmac_key, uint8_t **pub_key, size_t *pub_key_len)
{
    esp_err_t err = ESP_FAIL;
    psa_status_t status;
    psa_key_id_t key_id = 0;
    mbedtls_mpi ecc_priv_key;
    uint8_t priv_key_bytes[32];
    uint8_t raw_pub_key[65];  // 1 byte prefix + 32 bytes x + 32 bytes y
    size_t raw_pub_key_len;

    mbedtls_mpi_init(&ecc_priv_key);

    int ret = derive_ota_ecc_device_key(hmac_key, &ecc_priv_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to derive ECC device key: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Convert MPI to bytes
    ret = mbedtls_mpi_write_binary(&ecc_priv_key, priv_key_bytes, sizeof(priv_key_bytes));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to convert private key to bytes: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Import private key to PSA
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECDH);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);

    status = psa_import_key(&attributes, priv_key_bytes, sizeof(priv_key_bytes), &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to import ECC key to PSA: %d", (int)status);
        goto exit;
    }

    // Export public key (raw format: 0x04 || X || Y for uncompressed point)
    status = psa_export_public_key(key_id, raw_pub_key, sizeof(raw_pub_key), &raw_pub_key_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to export public key: %d", (int)status);
        goto exit;
    }

    // Convert raw public key to DER SubjectPublicKeyInfo format
    // Fixed ASN.1 header for secp256r1 EC public key
    static const uint8_t der_header[] = {
        0x30, 0x59,  // SEQUENCE, length 89
        0x30, 0x13,  // SEQUENCE, length 19
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,  // OID ecPublicKey (1.2.840.10045.2.1)
        0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,  // OID secp256r1 (1.2.840.10045.3.1.7)
        0x03, 0x42, 0x00  // BIT STRING, length 66, no unused bits
    };

    size_t der_len = sizeof(der_header) + raw_pub_key_len;
    *pub_key = calloc(1, der_len);
    if (*pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for public key");
        err = ESP_ERR_NO_MEM;
        goto exit;
    }

    memcpy(*pub_key, der_header, sizeof(der_header));
    memcpy(*pub_key + sizeof(der_header), raw_pub_key, raw_pub_key_len);
    *pub_key_len = der_len;

    ESP_LOGI(TAG, "ECC public key derived successfully");
    err = ESP_OK;

exit:
    if (key_id != 0) {
        psa_destroy_key(key_id);
    }
    mbedtls_mpi_free(&ecc_priv_key);
    mbedtls_platform_zeroize(priv_key_bytes, sizeof(priv_key_bytes));
    if (err != ESP_OK && *pub_key) {
        free(*pub_key);
        *pub_key = NULL;
        *pub_key_len = 0;
    }
    return err;
}
#else /* !CONFIG_MBEDTLS_VER_4_X_SUPPORT */
static esp_err_t esp_encrypted_img_export_ecies_pub_key(hmac_key_id_t hmac_key, uint8_t **pub_key, size_t *pub_key_len)
{
    esp_err_t err = ESP_FAIL;

    mbedtls_mpi ecc_priv_key;
    mbedtls_pk_context pk_ctx;
    mbedtls_ecp_keypair *ecp_keypair;

    mbedtls_mpi_init(&ecc_priv_key);
    mbedtls_pk_init(&pk_ctx);

    int ret = derive_ota_ecc_device_key(hmac_key, &ecc_priv_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to derive ECC device key: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Setup PK context for ECKEY
    ret = mbedtls_pk_setup(&pk_ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup PK context: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    ecp_keypair = mbedtls_pk_ec(pk_ctx);
    if (ecp_keypair == NULL) {
        ESP_LOGE(TAG, "Failed to get ECP keypair from PK context");
        goto exit;
    }

    // Load the curve
    ret = mbedtls_ecp_group_load(&ecp_keypair->MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to load ECP group: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Set the private key
    ret = mbedtls_mpi_copy(&ecp_keypair->MBEDTLS_PRIVATE(d), &ecc_priv_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to copy private key: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Compute the public key from private key
    ret = mbedtls_ecp_keypair_calc_public(ecp_keypair, mbedtls_esp_random, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to compute public key: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    // Public key will be stored in DER format, which includes ASN.1 headers.
    // For SECP256R1, the maximum DER public key size is:
    // 30 bytes (ASN.1 overhead) + 2 * 32 bytes (uncompressed coordinates) = 94 bytes
    size_t max_pubkey_len = DER_ASN1_OVERHEAD + (2 * SECP256R1_COORD_SIZE);
    *pub_key = calloc(1, max_pubkey_len);
    if (*pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for public key");
        err = ESP_ERR_NO_MEM;
        goto exit;
    }

    ret = mbedtls_pk_write_pubkey_der(&pk_ctx, *pub_key, max_pubkey_len);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write public key DER: -0x%04x", (unsigned int) - ret);
        goto exit;
    }

    *pub_key_len = ret;
    if (*pub_key_len > max_pubkey_len) {
        ESP_LOGE(TAG, "Public key length exceeds allocated buffer size");
        err = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    // Move the memory to the start of the buffer
    // mbedtls_pk_write_pubkey_der writes the key in reverse order, so we need to adjust the pointer
    // to point to the start of the key.
    memmove(*pub_key, *pub_key + (max_pubkey_len - ret), ret);

    // Resize buffer to actual size
    unsigned char *temp_pub_key = realloc(*pub_key, *pub_key_len);
    if (temp_pub_key == NULL) {
        ESP_LOGE(TAG, "Failed to resize public key buffer");
        err = ESP_ERR_NO_MEM;
        goto exit;
    }
    *pub_key = temp_pub_key;

    ESP_LOGI(TAG, "ECC public key derived successfully");
    err = ESP_OK;

exit:
    mbedtls_pk_free(&pk_ctx);
    mbedtls_mpi_free(&ecc_priv_key);
    if (err != ESP_OK && *pub_key) {
        free(*pub_key);
        *pub_key = NULL;
        *pub_key_len = 0;
    }
    return err;
}
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

esp_err_t esp_encrypted_img_export_public_key(esp_decrypt_handle_t ctx, uint8_t **pub_key, size_t *pub_key_len)
{
    if (ctx == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_export_public_key : Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    if (pub_key == NULL || pub_key_len == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_export_public_key : Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_encrypted_img_t *handle = (esp_encrypted_img_t *)ctx;

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if !defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    // In case of RSA, we return the public key corresponding to the private key
    // passed with esp_encrypted_img_decrypt_start()
    return esp_encrypted_img_export_rsa_pub_key(handle->rsa_pem, handle->rsa_len, pub_key, pub_key_len);
#else
    // In case of RSA with DS, the private key is stored securely in the DS context,
    // and we cannot export the public key directly.
    // The public key is derived from the DS context, so we return an error.
    (void)handle;
    ESP_LOGE(TAG, "Public key export is not supported for RSA with DS");
    return ESP_ERR_NOT_SUPPORTED;
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    // In case of ECIES, we return the public key corresponding to the private key
    // derived from the HMAC key ID passed with esp_encrypted_img_decrypt_start()
    return esp_encrypted_img_export_ecies_pub_key(handle->hmac_key, pub_key, pub_key_len);
#else
    ESP_LOGE(TAG, "No public key available for the current encryption scheme");
    return ESP_ERR_NOT_FOUND;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
}

esp_decrypt_handle_t esp_encrypted_img_decrypt_start(const esp_decrypt_cfg_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_start : Invalid argument");
        return NULL;
    }

    ESP_LOGI(TAG, "Initializing Decryption Handle");

    esp_encrypted_img_t *handle = calloc(1, sizeof(esp_encrypted_img_t));
    if (!handle) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle");
        goto failure;
    }

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    if (cfg->ds_data == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_start : Invalid argument");
        goto failure;
    }
#if CONFIG_MBEDTLS_VER_4_X_SUPPORT
    handle->ds_data = cfg->ds_data;
#else
    esp_err_t err = esp_ds_init_data_ctx(cfg->ds_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DS context, err: %2x", err);
        goto failure;
    }
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
#else
    if (cfg->rsa_priv_key == NULL || cfg->rsa_priv_key_len == 0) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_start : Invalid argument");
        goto failure;
    }

    handle->rsa_pem = calloc(1, cfg->rsa_priv_key_len);
    if (!handle->rsa_pem) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle->rsa_pem");
        goto failure;
    }

    memcpy(handle->rsa_pem, cfg->rsa_priv_key, cfg->rsa_priv_key_len);
    handle->rsa_len = cfg->rsa_priv_key_len;
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    if (cfg->hmac_key_id < 0 || cfg->hmac_key_id >= HMAC_KEY_MAX) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_start : Invalid argument");
        goto failure;
    }

    // Check if the HMAC key is burnt in eFuse
    if (!esp_encrypted_is_hmac_key_burnt_in_efuse(cfg->hmac_key_id)) {
        ESP_LOGE(TAG, "Could not find HMAC key in configured eFuse block!");
        goto failure;
    }

    handle->hmac_key = cfg->hmac_key_id;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */
    handle->cache_buf = calloc(1, ENC_GCM_KEY_SIZE);
    if (!handle->cache_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle->cache_buf");
        goto failure;
    }

#if defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed: %d", (int)status);
        goto failure;
    }
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */

    handle->state = ESP_PRE_ENC_IMG_READ_MAGIC;

    esp_decrypt_handle_t ctx = (esp_decrypt_handle_t)handle;
    return ctx;

failure:
    if (handle) {
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA) && !defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
        free(handle->rsa_pem);
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
        if (handle->cache_buf) {
            free(handle->cache_buf);
            handle->cache_buf = NULL;
        }
        free(handle);
    }
    return NULL;
}

static esp_err_t process_bin(esp_encrypted_img_t *handle, pre_enc_decrypt_arg_t *args, int curr_index)
{
    size_t data_len = args->data_in_len;
    size_t data_out_size = args->data_out_len;
    size_t output_len = 0;

    handle->binary_file_read += data_len - curr_index;
    int dec_len = 0;
    if (handle->binary_file_read != handle->binary_file_len) {
        size_t copy_len = 0;

        if ((handle->cache_buf_len + (data_len - curr_index)) - (handle->cache_buf_len + (data_len - curr_index)) % CACHE_BUF_SIZE > 0) {
            data_out_size = (handle->cache_buf_len + (data_len - curr_index)) - (handle->cache_buf_len + (data_len - curr_index)) % CACHE_BUF_SIZE;
            args->data_out = realloc(args->data_out, data_out_size);
            if (!args->data_out) {
                return ESP_ERR_NO_MEM;
            }
        }
        if (handle->cache_buf_len != 0) {
            copy_len = MIN(CACHE_BUF_SIZE - handle->cache_buf_len, data_len - curr_index);
            memcpy(handle->cache_buf + handle->cache_buf_len, args->data_in + curr_index, copy_len);
            handle->cache_buf_len += copy_len;
            if (handle->cache_buf_len != CACHE_BUF_SIZE) {
                args->data_out_len = 0;
                return ESP_ERR_NOT_FINISHED;
            }
            if (gcm_update(handle, (const unsigned char *)handle->cache_buf, CACHE_BUF_SIZE,
                           (unsigned char *)args->data_out, data_out_size, &output_len) != ESP_OK) {
                return ESP_FAIL;
            }
            dec_len = CACHE_BUF_SIZE;
        }
        handle->cache_buf_len = (data_len - curr_index - copy_len) % CACHE_BUF_SIZE;
        if (handle->cache_buf_len != 0) {
            data_len -= handle->cache_buf_len;
            memcpy(handle->cache_buf, args->data_in + (data_len), handle->cache_buf_len);
        }

        if (data_len - copy_len - curr_index > 0) {
            if (gcm_update(handle, (const unsigned char *)args->data_in + curr_index + copy_len,
                           data_len - copy_len - curr_index,
                           (unsigned char *)args->data_out + dec_len,
                           data_out_size - dec_len, &output_len) != ESP_OK) {
                return ESP_FAIL;
            }
        }
        args->data_out_len = dec_len + data_len - curr_index - copy_len;
        return ESP_ERR_NOT_FINISHED;
    }
    data_out_size = handle->cache_buf_len + data_len - curr_index;

    /* Handle zero-size allocation edge case to avoid undefined behavior */
    if (data_out_size == 0) {
        if (args->data_out) {
            free(args->data_out);
            args->data_out = NULL;
        }
        args->data_out_len = 0;
        return ESP_OK;
    }

    /* Use temporary pointer to prevent memory leak if realloc fails */
    void *temp = realloc(args->data_out, data_out_size);
    if (!temp) {
        /* Original pointer remains valid, caller should free it */
        return ESP_ERR_NO_MEM;
    }
    args->data_out = temp;
    size_t copy_len = 0;

    copy_len = MIN(CACHE_BUF_SIZE - handle->cache_buf_len, data_len - curr_index);
    memcpy(handle->cache_buf + handle->cache_buf_len, args->data_in + curr_index, copy_len);
    handle->cache_buf_len += copy_len;
    if (gcm_update(handle, (const unsigned char *)handle->cache_buf, handle->cache_buf_len,
                   (unsigned char *)args->data_out, data_out_size, &output_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (data_len - curr_index - copy_len > 0) {
        if (gcm_update(handle, (const unsigned char *)(args->data_in + curr_index + copy_len),
                       data_len - curr_index - copy_len,
                       (unsigned char *)(args->data_out + CACHE_BUF_SIZE),
                       data_out_size - CACHE_BUF_SIZE, &output_len) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    args->data_out_len = handle->cache_buf_len + data_len - copy_len - curr_index;
    handle->cache_buf_len = 0;

    return ESP_OK;
}

static void read_and_cache_data(esp_encrypted_img_t *handle, pre_enc_decrypt_arg_t *args, int *curr_index, int data_size)
{
    const int data_left = data_size - handle->binary_file_read;
    const int data_recv = args->data_in_len - *curr_index;
    if (handle->state == ESP_PRE_ENC_IMG_READ_IV) {
        memcpy(handle->iv + handle->cache_buf_len, args->data_in + *curr_index, MIN(data_recv, data_left));
    } else if (handle->state == ESP_PRE_ENC_IMG_READ_AUTH) {
        memcpy(handle->auth_tag + handle->cache_buf_len, args->data_in + *curr_index, MIN(data_recv, data_left));
    } else {
        memcpy(handle->cache_buf + handle->cache_buf_len, args->data_in + *curr_index, MIN(data_recv, data_left));
    }
    handle->cache_buf_len += MIN(data_recv, data_left);
    int temp = *curr_index;
    *curr_index += MIN(data_recv, data_left);
    handle->binary_file_read += MIN(args->data_in_len - temp, data_left);
}

static esp_err_t process_gcm_key(esp_encrypted_img_t *handle, const char *data_in, size_t data_in_len)
{
    if (data_in_len < ENC_GCM_KEY_SIZE) {
        ESP_LOGE(TAG, "GCM key size is less than expected");
        return ESP_FAIL;
    }
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    if (decipher_gcm_key(data_in, handle) != 0) {
        ESP_LOGE(TAG, "Unable to decipher GCM key");
        return ESP_FAIL;
    }
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    if (derive_gcm_key(data_in, handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to derive GCM key");
        return ESP_FAIL;
    }
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
    return ESP_OK;
}

esp_err_t esp_encrypted_img_decrypt_data(esp_decrypt_handle_t ctx, pre_enc_decrypt_arg_t *args)
{
    if (ctx == NULL || args == NULL || args->data_in == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_encrypted_img_t *handle = (esp_encrypted_img_t *)ctx;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_data: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    int curr_index = 0;

    switch (handle->state) {
    case ESP_PRE_ENC_IMG_READ_MAGIC:
        if (handle->cache_buf_len == 0 && (args->data_in_len - curr_index) >= MAGIC_SIZE) {
            uint32_t recv_magic = *(uint32_t *)args->data_in;
            if (recv_magic != esp_enc_img_magic) {
                ESP_LOGE(TAG, "Magic Verification failed");
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA) && !defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
                free(handle->rsa_pem);
                handle->rsa_pem = NULL;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
                return ESP_FAIL;
            }
            curr_index += MAGIC_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, MAGIC_SIZE);
            if (handle->binary_file_read == MAGIC_SIZE) {
                uint32_t recv_magic = *(uint32_t *)handle->cache_buf;

                if (recv_magic != esp_enc_img_magic) {
                    ESP_LOGE(TAG, "Magic Verification failed");
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA) && !defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
                    free(handle->rsa_pem);
                    handle->rsa_pem = NULL;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
                    return ESP_FAIL;
                }
                handle->binary_file_read = 0;
                handle->cache_buf_len = 0;
            } else {
                return ESP_ERR_NOT_FINISHED;
            }
        }
        ESP_LOGI(TAG, "Magic Verified");
        handle->state = ESP_PRE_ENC_IMG_READ_GCM;
    /* falls through */
    case ESP_PRE_ENC_IMG_READ_GCM:
        if (handle->cache_buf_len == 0 && args->data_in_len - curr_index >= ENC_GCM_KEY_SIZE) {
            if (process_gcm_key(handle, args->data_in + curr_index, ENC_GCM_KEY_SIZE) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process GCM key");
                handle->cache_buf_len = 0;
                return ESP_FAIL;
            }
            curr_index += ENC_GCM_KEY_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, ENC_GCM_KEY_SIZE);
            if (handle->cache_buf_len == ENC_GCM_KEY_SIZE) {
                if (process_gcm_key(handle, handle->cache_buf, ENC_GCM_KEY_SIZE) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to process GCM key");
                    handle->cache_buf_len = 0;
                    return ESP_FAIL;
                }
            } else {
                return ESP_ERR_NOT_FINISHED;
            }
        }
    /* falls through */
    case ESP_PRE_ENC_IMG_READ_IV:
        if (handle->cache_buf_len == 0 && args->data_in_len - curr_index >= IV_SIZE) {
            memcpy(handle->iv, args->data_in + curr_index, IV_SIZE);
            handle->binary_file_read = IV_SIZE;
            curr_index += IV_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, IV_SIZE);
        }
        if (handle->binary_file_read == IV_SIZE) {
            handle->state = ESP_PRE_ENC_IMG_READ_BINSIZE;
            handle->binary_file_read = 0;
            handle->cache_buf_len = 0;

            /* Initialize GCM with key and IV using abstraction layer */
            if (gcm_init_and_set_key(handle, (const unsigned char *)handle->gcm_key, GCM_KEY_SIZE * 8) != ESP_OK) {
                return ESP_FAIL;
            }
            if (gcm_start(handle, (const unsigned char *)handle->iv, IV_SIZE) != ESP_OK) {
                return ESP_FAIL;
            }
        } else {
            return ESP_ERR_NOT_FINISHED;
        }
    /* falls through */
    case ESP_PRE_ENC_IMG_READ_BINSIZE:
        if (handle->cache_buf_len == 0 && (args->data_in_len - curr_index) >= BIN_SIZE_DATA) {
            handle->binary_file_len = *(uint32_t *)(args->data_in + curr_index);
            curr_index += BIN_SIZE_DATA;
        } else {
            read_and_cache_data(handle, args, &curr_index, BIN_SIZE_DATA);
            if (handle->binary_file_read == BIN_SIZE_DATA) {
                handle->binary_file_len = *(uint32_t *)handle->cache_buf;
            } else {
                return ESP_ERR_NOT_FINISHED;
            }
        }
        handle->state = ESP_PRE_ENC_IMG_READ_AUTH;
        handle->binary_file_read = 0;
        handle->cache_buf_len = 0;
    /* falls through */
    case ESP_PRE_ENC_IMG_READ_AUTH:
        if (handle->cache_buf_len == 0 && args->data_in_len - curr_index >= AUTH_SIZE) {
            memcpy(handle->auth_tag, args->data_in + curr_index, AUTH_SIZE);
            handle->binary_file_read = AUTH_SIZE;
            curr_index += AUTH_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, AUTH_SIZE);
        }
        if (handle->binary_file_read == AUTH_SIZE) {
            handle->state = ESP_PRE_ENC_IMG_READ_EXTRA_HEADER;
            handle->binary_file_read = 0;
            handle->cache_buf_len = 0;
        } else {
            return ESP_ERR_NOT_FINISHED;
        }
    /* falls through */
    case ESP_PRE_ENC_IMG_READ_EXTRA_HEADER: {
        int temp = curr_index;
        curr_index += MIN(args->data_in_len - curr_index, RESERVED_HEADER - handle->binary_file_read);
        handle->binary_file_read += MIN(args->data_in_len - temp, RESERVED_HEADER - handle->binary_file_read);
        if (handle->binary_file_read == RESERVED_HEADER) {
            handle->state = ESP_PRE_ENC_DATA_DECODE_STATE;
            handle->binary_file_read = 0;
            handle->cache_buf_len = 0;
        } else {
            return ESP_ERR_NOT_FINISHED;
        }
    }
    /* falls through */
    case ESP_PRE_ENC_DATA_DECODE_STATE:
        err = process_bin(handle, args, curr_index);
        return err;
    }
    return ESP_OK;
}

esp_err_t esp_encrypted_img_decrypt_end(esp_decrypt_handle_t ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_encrypted_img_t *handle = (esp_encrypted_img_t *)ctx;
    esp_err_t err = ESP_OK;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_data: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state == ESP_PRE_ENC_DATA_DECODE_STATE) {
        if (handle->cache_buf_len != 0 || handle->binary_file_read != handle->binary_file_len) {
            ESP_LOGE(TAG, "Invalid operation");
            err = ESP_FAIL;
            goto exit;
        }

        /* Verify authentication tag using abstraction layer */
        if (gcm_finish_and_verify(handle, (const unsigned char *)handle->auth_tag, AUTH_SIZE) != ESP_OK) {
            err = ESP_FAIL;
            goto exit;
        }
    } else {
        // If the state is not ESP_PRE_ENC_DATA_DECODE_STATE, it means that the
        // decryption process was not completed successfully.
        ESP_LOGE(TAG, "Decryption process not completed successfully");
        err = ESP_FAIL;
        goto exit;
    }
    err = ESP_OK;
exit:
    gcm_cleanup(handle);
    free(handle->cache_buf);
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    esp_ds_deinit_data_ctx();
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
#else
    free(handle->rsa_pem);
    handle->rsa_pem = NULL;
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
    free(handle);
    return err;
}

bool esp_encrypted_img_is_complete_data_received(esp_decrypt_handle_t ctx)
{
    esp_encrypted_img_t *handle = (esp_encrypted_img_t *)ctx;
    return (handle != NULL && handle->binary_file_len == handle->binary_file_read);
}

esp_err_t esp_encrypted_img_decrypt_abort(esp_decrypt_handle_t ctx)
{
    esp_encrypted_img_t *handle = (esp_encrypted_img_t *)ctx;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_abort: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    gcm_cleanup(handle);
    free(handle->cache_buf);
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
#if !defined(CONFIG_MBEDTLS_VER_4_X_SUPPORT)
    esp_ds_deinit_data_ctx();
#endif /* CONFIG_MBEDTLS_VER_4_X_SUPPORT */
#else
    free(handle->rsa_pem);
    handle->rsa_pem = NULL;
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
    free(handle);
    return ESP_OK;
}

uint16_t esp_encrypted_img_get_header_size(void)
{
    return HEADER_DATA_SIZE;
}
