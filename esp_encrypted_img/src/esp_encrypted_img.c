/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_encrypted_img.h"
#include <errno.h>
#include <esp_log.h>
#include <esp_err.h>

#include "mbedtls/version.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/gcm.h"
#include "sys/param.h"

static const char *TAG = "esp_encrypted_img";

typedef enum {
    ESP_PRE_ENC_IMG_READ_MAGIC,
    ESP_PRE_ENC_IMG_READ_GCM,
    ESP_PRE_ENC_IMG_READ_IV,
    ESP_PRE_ENC_IMG_READ_BINSIZE,
    ESP_PRE_ENC_IMG_READ_AUTH,
    ESP_PRE_ENC_IMG_READ_EXTRA_HEADER,
    ESP_PRE_ENC_DATA_DECODE_STATE,
} esp_encrypted_img_state;

struct esp_encrypted_img_handle {
    const char *rsa_pem;
    size_t rsa_len;
    uint32_t binary_file_len;
    uint32_t binary_file_read;
    char *gcm_key;
    char *iv;
    char auth_tag[16];
    esp_encrypted_img_state state;
    mbedtls_gcm_context gcm_ctx;
    size_t cache_buf_len;
    char *cache_buf;
};

#define GCM_KEY_SIZE        32
#define MAGIC_SIZE          4
#define ENC_GCM_KEY_SIZE    384
#define IV_SIZE             16
#define BIN_SIZE_DATA       4
#define AUTH_SIZE           16
#define RESERVED_HEADER     88

typedef struct {
    char magic[MAGIC_SIZE];
    char enc_gcm[ENC_GCM_KEY_SIZE];
    char iv[IV_SIZE];
    char bin_size[BIN_SIZE_DATA];
    char auth[AUTH_SIZE];
    char extra_header[RESERVED_HEADER];
} pre_enc_bin_header;
#define HEADER_DATA_SIZE    sizeof(pre_enc_bin_header)

// Magic Byte is created using command: echo -n "esp_encrypted_img" | sha256sum
static uint32_t esp_enc_img_magic = 0x0788b6cf;

typedef struct esp_encrypted_img_handle esp_encrypted_img_t;

static int decipher_gcm_key(char *enc_gcm, esp_encrypted_img_t *handle)
{
    int ret = 1;
    handle->gcm_key = calloc(1, GCM_KEY_SIZE);
    if (!handle->gcm_key) {
        return ESP_ERR_NO_MEM;
    }
    size_t olen = 0;
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
        free(handle->gcm_key);
        goto exit;
    }

    ESP_LOGI(TAG, "Reading RSA private key");

#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) handle->rsa_pem, handle->rsa_len, NULL, 0)) != 0) {
#else
    if ( (ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) handle->rsa_pem, handle->rsa_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
#endif
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_parse_keyfile returned -0x%04x\n", (unsigned int) - ret );
        free(handle->gcm_key);
        goto exit;
    }

    if (( ret = mbedtls_pk_decrypt( &pk, (const unsigned char *)enc_gcm, ENC_GCM_KEY_SIZE, (unsigned char *)handle->gcm_key, &olen, GCM_KEY_SIZE,
                                    mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 ) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_pk_decrypt returned -0x%04x\n", (unsigned int) - ret );
        free(handle->gcm_key);
        goto exit;
    }
    handle->cache_buf = realloc(handle->cache_buf, 16);
    if (!handle->cache_buf) {
        return ESP_ERR_NO_MEM;
    }
    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;
    handle->iv = calloc(1, IV_SIZE);
    if (!handle->iv) {
        return ESP_ERR_NO_MEM;
    }

exit:
    mbedtls_pk_free( &pk );
    mbedtls_entropy_free( &entropy );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    free((void *)handle->rsa_pem);

    return (ret);
}

esp_decrypt_handle_t esp_encrypted_img_decrypt_start(const esp_decrypt_cfg_t *cfg)
{
    if (cfg == NULL || cfg->rsa_pub_key == NULL) {
        ESP_LOGE(TAG, "esp_encrypted_img_decrypt_start : Invalid argument");
        return NULL;
    }
    ESP_LOGI(TAG, "Starting Decryption Process");

    esp_encrypted_img_t *handle = calloc(1, sizeof(esp_encrypted_img_t));
    if (!handle) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle");
        goto failure;
    }

    handle->rsa_pem = calloc(1, cfg->rsa_pub_key_len);
    if (!handle->rsa_pem) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle->rsa_pem");
        goto failure;
    }

    handle->cache_buf = calloc(1, ENC_GCM_KEY_SIZE);
    if (!handle->cache_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to handle->cache_buf");
        goto failure;
    }

    memcpy((void *)handle->rsa_pem, cfg->rsa_pub_key, cfg->rsa_pub_key_len);
    handle->rsa_len = cfg->rsa_pub_key_len;
    handle->state = ESP_PRE_ENC_IMG_READ_MAGIC;

    esp_decrypt_handle_t *ctx = (esp_decrypt_handle_t *)handle;
    return ctx;

failure:
    if (!handle) {
        return NULL;
    }
    if (handle->rsa_pem) {
        free((void *)handle->rsa_pem);
    }
    if (handle) {
        free(handle);
    }
    return NULL;
}

static esp_err_t process_bin(esp_encrypted_img_t *handle, pre_enc_decrypt_arg_t *args, int curr_index)
{
    size_t data_len = args->data_in_len;
    size_t data_out_size = args->data_out_len;
    size_t olen;
    handle->binary_file_read += data_len - curr_index;
    int dec_len = 0;
    if (handle->binary_file_read != handle->binary_file_len) {
        size_t copy_len = 0;

        if ((handle->cache_buf_len + (data_len - curr_index)) - (handle->cache_buf_len + (data_len - curr_index)) % 16 > 0) {
            data_out_size = (handle->cache_buf_len + (data_len - curr_index)) - (handle->cache_buf_len + (data_len - curr_index)) % 16;
            args->data_out = realloc(args->data_out, data_out_size);
            if (!args->data_out) {
                return ESP_ERR_NO_MEM;
            }
        }
        if (handle->cache_buf_len != 0) {
            copy_len = MIN(16 - handle->cache_buf_len, data_len - curr_index);
            memcpy(handle->cache_buf + handle->cache_buf_len, args->data_in + curr_index, copy_len);
            handle->cache_buf_len += copy_len;
            if (handle->cache_buf_len != 16) {
                args->data_out_len = 0;
                return ESP_ERR_NOT_FINISHED;
            }
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
            if (mbedtls_gcm_update(&handle->gcm_ctx, 16, (const unsigned char *)handle->cache_buf, (unsigned char *) args->data_out) != 0) {
#else
            if (mbedtls_gcm_update(&handle->gcm_ctx, (const unsigned char *)handle->cache_buf, 16, (unsigned char *) args->data_out, data_out_size, &olen) != 0) {
#endif
                return ESP_FAIL;
            }
            dec_len = 16;
        }
        handle->cache_buf_len = (data_len - curr_index - copy_len) % 16;
        if (handle->cache_buf_len != 0) {
            data_len -= handle->cache_buf_len;
            memcpy(handle->cache_buf, args->data_in + (data_len), handle->cache_buf_len);
        }

        if (data_len - copy_len - curr_index > 0) {
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
            if (mbedtls_gcm_update(&handle->gcm_ctx, data_len - copy_len - curr_index, (const unsigned char *)args->data_in + curr_index + copy_len, (unsigned char *)args->data_out + dec_len) != 0) {
#else
            if (mbedtls_gcm_update(&handle->gcm_ctx, (const unsigned char *)args->data_in + curr_index + copy_len, data_len - copy_len - curr_index, (unsigned char *)args->data_out + dec_len, data_out_size - dec_len, &olen) != 0) {
#endif
                return ESP_FAIL;
            }
        }
        args->data_out_len = dec_len + data_len - curr_index - copy_len;
        return ESP_ERR_NOT_FINISHED;
    }
    data_out_size = handle->cache_buf_len + data_len - curr_index;
    args->data_out = realloc(args->data_out, data_out_size);
    if (!args->data_out) {
        return ESP_ERR_NO_MEM;
    }
    size_t copy_len = 0;

    copy_len = MIN(16 - handle->cache_buf_len, data_len - curr_index);
    memcpy(handle->cache_buf + handle->cache_buf_len, args->data_in + curr_index, copy_len);
    handle->cache_buf_len += copy_len;
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    if (mbedtls_gcm_update(&handle->gcm_ctx, handle->cache_buf_len, (const unsigned char *)handle->cache_buf, (unsigned char *)args->data_out) != 0) {
#else
    if (mbedtls_gcm_update(&handle->gcm_ctx,  (const unsigned char *)handle->cache_buf, handle->cache_buf_len, (unsigned char *)args->data_out, data_out_size, &olen) != 0) {
#endif
        return ESP_FAIL;
    }
    if (data_len - curr_index - copy_len > 0) {
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
        if (mbedtls_gcm_update(&handle->gcm_ctx, data_len - curr_index - copy_len, (const unsigned char *)(args->data_in + curr_index + copy_len), (unsigned char *)(args->data_out + 16)) != 0) {
#else
        if (mbedtls_gcm_update(&handle->gcm_ctx,  (const unsigned char *)(args->data_in + curr_index + copy_len), data_len - curr_index - copy_len, (unsigned char *)(args->data_out + 16), data_out_size - 16, &olen) != 0) {
#endif
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
    if (handle->state == ESP_PRE_ENC_IMG_READ_IV && handle->iv) {
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

esp_err_t esp_encrypted_img_decrypt_data(esp_decrypt_handle_t *ctx, pre_enc_decrypt_arg_t *args)
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
                free((void *)handle->rsa_pem);
                return ESP_FAIL;
            }
            curr_index += MAGIC_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, MAGIC_SIZE);
            if (handle->binary_file_read == MAGIC_SIZE) {
                uint32_t recv_magic = *(uint32_t *)handle->cache_buf;

                if (recv_magic != esp_enc_img_magic) {
                    ESP_LOGE(TAG, "Magic Verification failed");
                    free((void *)handle->rsa_pem);
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
            if (decipher_gcm_key(args->data_in + curr_index, handle) != 0) {
                ESP_LOGE(TAG, "Unable to decipher GCM key");
                return ESP_FAIL;
            }
            curr_index += ENC_GCM_KEY_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, ENC_GCM_KEY_SIZE);
            if (handle->cache_buf_len == ENC_GCM_KEY_SIZE) {
                if (decipher_gcm_key(handle->cache_buf, handle) != 0) {
                    ESP_LOGE(TAG, "Unable to decipher GCM key");
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
            mbedtls_gcm_init(&handle->gcm_ctx);
            if ((err = mbedtls_gcm_setkey(&handle->gcm_ctx, MBEDTLS_CIPHER_ID_AES, (const unsigned char *)handle->gcm_key, GCM_KEY_SIZE * 8)) != 0) {
                ESP_LOGE(TAG, "Error: mbedtls_gcm_set_key: -0x%04x\n", (unsigned int) - err);
                return ESP_FAIL;
            }
            free(handle->gcm_key);
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
            if (mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, (const unsigned char *)handle->iv, IV_SIZE, NULL, 0) != 0) {
#else
            if (mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, (const unsigned char *)handle->iv, IV_SIZE) != 0) {
#endif
                ESP_LOGE(TAG, "Error: mbedtls_gcm_starts: -0x%04x\n", (unsigned int) - err);
                return ESP_FAIL;
            }
            free(handle->iv);
            handle->iv = NULL;
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

esp_err_t esp_encrypted_img_decrypt_end(esp_decrypt_handle_t *ctx)
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

        char *got_auth = calloc(1, AUTH_SIZE);
        if (!got_auth) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            err = ESP_FAIL;
            goto exit;
        }
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
        err = mbedtls_gcm_finish(&handle->gcm_ctx, (unsigned char *)got_auth, AUTH_SIZE);
#else
        size_t olen;
        err = mbedtls_gcm_finish(&handle->gcm_ctx, NULL, 0, &olen, (unsigned char *)got_auth, AUTH_SIZE);
#endif
        if (err != 0) {
            ESP_LOGE(TAG, "Error: %d", err);
            free(got_auth);
            err = ESP_FAIL;
            goto exit;
        }
        if (memcmp(got_auth, handle->auth_tag, AUTH_SIZE) != 0) {
            ESP_LOGE(TAG, "Invalid Auth");
            free(got_auth);
            err = ESP_FAIL;
            goto exit;
        }

        free(got_auth);
    }
    err = ESP_OK;
exit:
    mbedtls_gcm_free(&handle->gcm_ctx);
    free(handle->cache_buf);
    free(handle);
    return err;
}
