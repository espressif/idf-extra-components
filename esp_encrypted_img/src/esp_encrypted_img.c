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

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/hkdf.h"
#include "esp_random.h"
#include "esp_encrypted_img_utilities.h"
#if SOC_HMAC_SUPPORTED


#include "esp_efuse.h"
#include "esp_efuse_chip.h"
#endif /* SOC_HMAC_SUPPORTED */
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

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

#define GCM_KEY_SIZE        32

struct esp_encrypted_img_handle {
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    char *rsa_pem;
    size_t rsa_len;
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    hmac_key_id_t hmac_key;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */
    uint32_t binary_file_len;
    uint32_t binary_file_read;
    char gcm_key[GCM_KEY_SIZE];
    char iv[IV_SIZE];
    char auth_tag[AUTH_SIZE];
    esp_encrypted_img_state state;
    mbedtls_gcm_context gcm_ctx;
    size_t cache_buf_len;
    char *cache_buf;
};

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
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

typedef struct esp_encrypted_img_handle esp_encrypted_img_t;

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
static int decipher_gcm_key(const char *enc_gcm, esp_encrypted_img_t *handle)
{
    int ret = 1;
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
    handle->cache_buf = realloc(handle->cache_buf, 16);
    if (!handle->cache_buf) {
        return ESP_ERR_NO_MEM;
    }
    handle->state = ESP_PRE_ENC_IMG_READ_IV;
    handle->binary_file_read = 0;
    handle->cache_buf_len = 0;
exit:
    mbedtls_pk_free( &pk );
    mbedtls_entropy_free( &entropy );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    free(handle->rsa_pem);
    handle->rsa_pem = NULL;

    return (ret);
}
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */

#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)

static uint8_t pbkdf2_salt[32] = {
    0x0e, 0x21, 0x60, 0x64, 0x2d, 0xae, 0x76, 0xd3, 0x34, 0x48, 0xe4, 0x3d, 0x77, 0x20, 0x12, 0x3d,
    0x9f, 0x3b, 0x1e, 0xce, 0xb8, 0x8e, 0x57, 0x3a, 0x4e, 0x8f, 0x7f, 0xb9, 0x4f, 0xf0, 0xc8, 0x69
};

static int mbedtls_esp_random(void *ctx, unsigned char *buf, size_t len)
{
    esp_fill_random(buf, len);
    return 0;
}

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
    // we wil check again here to ensure that the HMAC key is valid.
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

    uint8_t shared_secret_bytes[32] = {0};
    mbedtls_mpi shared_secret;
    mbedtls_mpi_init(&shared_secret);

    if ((ret = mbedtls_ecdh_compute_shared(&grp, &shared_secret, server_public_point, &device_private_mpi,
                                           mbedtls_esp_random, NULL)) != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_ecdh_compute_shared returned -0x%04x\n", (unsigned int) - ret);
        mbedtls_mpi_free(&shared_secret);
        mbedtls_mpi_init(&shared_secret);
        memset(&device_private_mpi, 0, sizeof(mbedtls_mpi));
        mbedtls_mpi_free(&device_private_mpi);
        goto exit;
    }
    memset(&device_private_mpi, 0, sizeof(mbedtls_mpi));
    mbedtls_mpi_free(&device_private_mpi);

    if ((ret = mbedtls_mpi_write_binary(&shared_secret, shared_secret_bytes, sizeof(shared_secret_bytes))) != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_mpi_write_binary returned -0x%04x\n", (unsigned int) - ret);
        mbedtls_mpi_free(&shared_secret); // Free shared_secret on error
        goto exit;
    }
    mbedtls_platform_zeroize(&shared_secret, sizeof(shared_secret));
    mbedtls_mpi_free(&shared_secret);

    unsigned char *hkdf_info = calloc(1, HKDF_INFO_SIZE);
    if (hkdf_info == NULL) {
        ESP_LOGE(TAG, "failed to allocate memory for hkdf_info");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    memcpy(hkdf_info, "_esp_enc_img_ecc", HKDF_INFO_SIZE);;

    ret = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), kdf_salt, KDF_SALT_SIZE,
                       (const unsigned char *)shared_secret_bytes, sizeof(shared_secret_bytes),
                       hkdf_info, HKDF_INFO_SIZE, derived_key, GCM_KEY_SIZE);
    mbedtls_platform_zeroize(hkdf_info, HKDF_INFO_SIZE);
    free(hkdf_info);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed\n  ! mbedtls_hkdf returned -0x%04x\n", (unsigned int) - ret);
        goto exit;
    }

    memcpy(handle->gcm_key, derived_key, GCM_KEY_SIZE);
    ESP_LOGI(TAG, "GCM key derived successfully");
    handle->cache_buf = realloc(handle->cache_buf, 16);
    if (!handle->cache_buf) {
        return ESP_ERR_NO_MEM;
    }
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

#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES */

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
    handle->state = ESP_PRE_ENC_IMG_READ_MAGIC;

    esp_decrypt_handle_t ctx = (esp_decrypt_handle_t)handle;
    return ctx;

failure:
    if (handle) {
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
        free(handle->rsa_pem);
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
        if (handle->cache_buf) {
            free(handle->cache_buf);
        }
        free(handle);
    }
    return NULL;
}

static esp_err_t process_bin(esp_encrypted_img_t *handle, pre_enc_decrypt_arg_t *args, int curr_index)
{
    size_t data_len = args->data_in_len;
    size_t data_out_size = args->data_out_len;
#if !(MBEDTLS_VERSION_NUMBER < 0x03000000)
    size_t olen;
#endif
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
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
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
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
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
            process_gcm_key(handle, args->data_in + curr_index, ENC_GCM_KEY_SIZE);
            curr_index += ENC_GCM_KEY_SIZE;
        } else {
            read_and_cache_data(handle, args, &curr_index, ENC_GCM_KEY_SIZE);
            if (handle->cache_buf_len == ENC_GCM_KEY_SIZE) {
                process_gcm_key(handle, handle->cache_buf, ENC_GCM_KEY_SIZE);
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
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
            if (mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, (const unsigned char *)handle->iv, IV_SIZE, NULL, 0) != 0) {
#else
            if (mbedtls_gcm_starts(&handle->gcm_ctx, MBEDTLS_GCM_DECRYPT, (const unsigned char *)handle->iv, IV_SIZE) != 0) {
#endif
                ESP_LOGE(TAG, "Error: mbedtls_gcm_starts: -0x%04x\n", (unsigned int) - err);
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

        unsigned char got_auth[AUTH_SIZE] = {0};
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
        err = mbedtls_gcm_finish(&handle->gcm_ctx, got_auth, AUTH_SIZE);
#else
        size_t olen;
        err = mbedtls_gcm_finish(&handle->gcm_ctx, NULL, 0, &olen, got_auth, AUTH_SIZE);
#endif
        if (err != 0) {
            ESP_LOGE(TAG, "Error: %d", err);
            err = ESP_FAIL;
            goto exit;
        }
        if (memcmp(got_auth, handle->auth_tag, AUTH_SIZE) != 0) {
            ESP_LOGE(TAG, "Invalid Auth");
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
    mbedtls_gcm_free(&handle->gcm_ctx);
    free(handle->cache_buf);
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    free(handle->rsa_pem);
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
    mbedtls_gcm_free(&handle->gcm_ctx);
    free(handle->cache_buf);
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    free(handle->rsa_pem);
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */
    free(handle);
    return ESP_OK;
}

uint16_t esp_encrypted_img_get_header_size(void)
{
    return HEADER_DATA_SIZE;
}
