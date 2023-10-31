/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"

#include "freertos/FreeRTOS.h"

#include "unity.h"
#include "l8w8jwt/encode.h"
#include "l8w8jwt/decode.h"

#define TEST_ASSERT_MBEDTLS_OK(X)  TEST_ASSERT_EQUAL_HEX32(0, -(X))

#define ECDSA_KEYS_BUF_SIZE        (256)

static void ecdsa_256_genkey(unsigned char *pvtkey, size_t pvtkey_size, unsigned char *pubkey, size_t pubkey_size)
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;

    mbedtls_pk_context key;
    const char *p_str = "myecdsa";

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_pk_init(&key);

    TEST_ASSERT_MBEDTLS_OK(mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, (const unsigned char *) p_str, strlen(p_str)));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_ecdsa_genkey(mbedtls_pk_ec(key), MBEDTLS_ECP_DP_SECP256R1, mbedtls_ctr_drbg_random, &random));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_pk_write_key_pem(&key, pvtkey, pvtkey_size));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_pk_write_pubkey_pem(&key, pubkey, pubkey_size));

    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
}

int example_jwt_decode(char *jwt, const unsigned char *pubkey, enum l8w8jwt_validation_result *validation_result)
{
    struct l8w8jwt_decoding_params params;
    l8w8jwt_decoding_params_init(&params);

    params.alg = L8W8JWT_ALG_ES256;

    params.jwt = jwt;
    params.jwt_length = strlen(jwt);

    params.verification_key = (unsigned char *)pubkey;
    params.verification_key_length = strlen((const char *)pubkey);

    params.validate_iss = "Black Mesa";
    params.validate_iss_length = strlen(params.validate_iss);

    params.validate_sub = "Gordon Freeman";
    params.validate_sub_length = strlen(params.validate_sub);

    params.validate_exp = 1;
    params.exp_tolerance_seconds = 60;

    params.validate_iat = 1;
    params.iat_tolerance_seconds = 0;

    int ret = l8w8jwt_decode(&params, validation_result, NULL, NULL);
    printf("%d\n", *validation_result);
    return ret;
}

int example_jwt_encode(const unsigned char *pvtkey, char **jwt)
{
    size_t jwt_length;

    struct l8w8jwt_claim payload_claims[] = {
        {
            .key = "ctx",
            .key_length = 3,
            .value = "Unforseen Consequences",
            .value_length = strlen("Unforseen Consequences"),
            .type = L8W8JWT_CLAIM_TYPE_STRING
        },
    };

    struct l8w8jwt_encoding_params params;
    l8w8jwt_encoding_params_init(&params);

    params.alg = L8W8JWT_ALG_ES256;

    params.sub = "Gordon Freeman";
    params.sub_length = strlen("Gordon Freeman");

    params.iss = "Black Mesa";
    params.iss_length = strlen("Black Mesa");

    params.aud = "Administrator";
    params.aud_length = strlen("Administrator");

    params.iat = time(NULL);
    params.exp = time(NULL) + 600; // Set to expire after 10 minutes (600 seconds).

    params.additional_payload_claims = payload_claims;
    params.additional_payload_claims_count = sizeof(payload_claims) / sizeof(struct l8w8jwt_claim);

    params.secret_key = (unsigned char *)pvtkey;
    params.secret_key_length = strlen((const char *)pvtkey);

    params.out = jwt;
    params.out_length = &jwt_length;

    return l8w8jwt_encode(&params);
}

TEST_CASE("Verify encoded and signed JWT", "[l8w8jwt]")
{
    static unsigned char pvtkey[ECDSA_KEYS_BUF_SIZE], pubkey[ECDSA_KEYS_BUF_SIZE];
    ecdsa_256_genkey(pvtkey, sizeof(pvtkey), pubkey, sizeof(pubkey));

    char *jwt = NULL;
    TEST_ASSERT_EQUAL_HEX32(L8W8JWT_SUCCESS, example_jwt_encode(pvtkey, &jwt));
    TEST_ASSERT_NOT_NULL(jwt);

    enum l8w8jwt_validation_result validation_result = L8W8JWT_NBF_FAILURE;
    TEST_ASSERT_EQUAL_HEX32(L8W8JWT_SUCCESS, example_jwt_decode(jwt, pubkey, &validation_result));
    TEST_ASSERT_EQUAL_HEX32(L8W8JWT_VALID, validation_result);

    /* Never forget to free the jwt string! */
    l8w8jwt_free(jwt);
}
