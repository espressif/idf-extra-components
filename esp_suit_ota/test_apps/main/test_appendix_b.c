/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* The six signed examples of draft-ietf-suit-manifest-38 Appendix B, byte for byte. */
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "esp_suit_ota.h"
#include "suit_priv.h"
#include "test_common.h"
#include "vectors.h"

static const esp_suit_ota_key_t s_spec_key = { .point = suit_spec_pubkey, .point_len = 65 };

static int fake_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap)
{
    return -ESP_FAIL;
}

/* split the envelope into auth and manifest bstr (with head) the way the framer does */
static void split_envelope(const uint8_t *env, size_t len, suit_span_t *auth, suit_span_t *manifest)
{
    suit_cbor_t c;
    suit_cbor_init(&c, env, len);
    uint64_t tag;
    size_t n;
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_tag(&c, &tag));
    TEST_ASSERT_EQUAL(107, tag);
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_map(&c, &n));
    uint64_t k;
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_uint(&c, &k));
    TEST_ASSERT_EQUAL(2, k);
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_bstr(&c, &auth->ptr, &auth->len));
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_uint(&c, &k));
    TEST_ASSERT_EQUAL(3, k);
    TEST_ASSERT_EQUAL(ESP_OK, suit_cbor_get_raw(&c, &manifest->ptr, &manifest->len));
}

TEST_CASE("appendix B: every example verifies with the published key", "[appendix_b]")
{
    for (int i = 0; i < 6; i++) {
        suit_span_t auth, man;
        split_envelope(suit_vectors[i].data, suit_vectors[i].len, &auth, &man);
        uint8_t digest[32];
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, suit_cose_verify(auth.ptr, auth.len, man.ptr, man.len, &s_spec_key, 1, digest),
                                  "vector verification");
    }
}

TEST_CASE("appendix B: another key, a changed signature or a changed manifest is refused", "[appendix_b]")
{
    suit_span_t auth, man;
    split_envelope(suit_vectors[1].data, suit_vectors[1].len, &auth, &man);
    uint8_t digest[32];
    uint8_t point[65];
    memcpy(point, suit_spec_pubkey, 65);
    point[64] ^= 0x01;                 /* not on the curve, or another key: either way not trusted */
    esp_suit_ota_key_t other = { .point = point, .point_len = 65 };
    TEST_ASSERT_NOT_EQUAL(ESP_OK, suit_cose_verify(auth.ptr, auth.len, man.ptr, man.len, &other, 1, digest));

    uint8_t *copy = malloc(auth.len);
    memcpy(copy, auth.ptr, auth.len);
    copy[auth.len - 1] ^= 0x01;        /* last byte of the signature */
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_AUTH, suit_cose_verify(copy, auth.len, man.ptr, man.len, &s_spec_key, 1, digest));
    free(copy);

    uint8_t *mcopy = malloc(man.len);
    memcpy(mcopy, man.ptr, man.len);
    mcopy[man.len - 1] ^= 0x01;        /* manifest changed after signing */
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_AUTH, suit_cose_verify(auth.ptr, auth.len, mcopy, man.len, &s_spec_key, 1, digest));
    free(mcopy);
}

TEST_CASE("appendix B: every example parses", "[appendix_b]")
{
    static const int expect_comps[6] = { 1, 1, 1, 1, 3, 2 };
    for (int i = 0; i < 6; i++) {
        suit_span_t auth, man;
        split_envelope(suit_vectors[i].data, suit_vectors[i].len, &auth, &man);
        suit_cbor_head_t h;
        int hl = suit_cbor_head(man.ptr, man.len, &h);
        TEST_ASSERT_GREATER_THAN(0, hl);
        suit_manifest_t m;
        TEST_ASSERT_EQUAL(ESP_OK, suit_manifest_parse(man.ptr + hl, man.len - hl, &m));
        TEST_ASSERT_EQUAL(i, (int)m.sequence_number);
        TEST_ASSERT_EQUAL(expect_comps[i], (int)m.num_components);
        TEST_ASSERT_NOT_NULL(m.shared_seq.ptr);
    }
}

static esp_err_t feed_all(esp_suit_ota_handle_t h, const uint8_t *data, size_t len, size_t chunk)
{
    size_t pos = 0;
    esp_err_t err = ESP_OK;
    while (pos < len) {
        size_t n = len - pos < chunk ? len - pos : chunk;
        size_t used = 0;
        err = esp_suit_ota_feed(h, data + pos, n, &used);
        if (err != ESP_OK) {
            return err;
        }
        TEST_ASSERT_EQUAL(n, used);
        pos += n;
    }
    return err;
}

static esp_err_t run_example(int i, size_t chunk, esp_suit_ota_transport_t *t, esp_suit_ota_handle_t *out)
{
    test_reset_state();
    memset(t, 0, sizeof(*t));
    t->https.get = fake_get;
    t->url = "http://example.com/manifest.suit";
    esp_suit_ota_handle_t h;
    esp_err_t err = esp_suit_ota_begin(&g_cfg, t, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = feed_all(h, suit_vectors[i].data, suit_vectors[i].len, chunk);
    if (out != NULL && err == ESP_OK) {
        *out = h;
    } else {
        esp_suit_ota_abort(h);
    }
    return err;
}

TEST_CASE("appendix B: Example 0 is a boot manifest, outside an updater's role", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_UNSUPPORTED, run_example(0, 4096, &t, NULL));
}

TEST_CASE("appendix B: Example 1 redirects the transport to file.bin", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, run_example(1, 4096, &t, &h));
    TEST_ASSERT_EQUAL_STRING("http://example.com/file.bin", t.url);
    TEST_ASSERT_EQUAL(34768, t.len);
    TEST_ASSERT_EQUAL(0, t.component);
    TEST_ASSERT_FALSE(t.done);
    esp_suit_ota_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_get_info(h, &info));
    TEST_ASSERT_EQUAL(1, info.sequence_number);
    TEST_ASSERT_EQUAL(1, info.num_pending);
    /* the payload has an unknown preimage: wrong bytes must fail at the boundary */
    uint8_t *junk = calloc(1, 34768);
    size_t used = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_DIGEST, esp_suit_ota_feed(h, junk, 34768, &used));
    free(junk);
    esp_suit_ota_abort(h);
}

TEST_CASE("appendix B: Example 1 gives the same result at every chunk size", "[appendix_b]")
{
    for (size_t chunk = 1; chunk <= suit_vectors[1].len; chunk++) {
        esp_suit_ota_transport_t t;
        esp_suit_ota_handle_t h;
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, run_example(1, chunk, &t, &h), "chunked feed");
        TEST_ASSERT_EQUAL_STRING("http://example.com/file.bin", t.url);
        TEST_ASSERT_EQUAL(34768, t.len);
        esp_suit_ota_abort(h);
    }
}

TEST_CASE("appendix B: Example 2 severed install and text", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, run_example(2, 64, &t, &h));
    TEST_ASSERT_NOT_NULL(strstr(t.url, "http://example.com/"));
    TEST_ASSERT_NOT_NULL(strstr(t.url, "/file.bin"));
    TEST_ASSERT_EQUAL(34768, t.len);
    esp_suit_ota_abort(h);
}

TEST_CASE("appendix B: Example 3 try-each picks the slot 0 branch", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, run_example(3, 4096, &t, &h));
    TEST_ASSERT_NOT_NULL(strstr(t.url, "example.com"));
    esp_suit_ota_abort(h);
}

TEST_CASE("appendix B: Example 4 fetches without an image size, outside this profile", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_SIZE, run_example(4, 4096, &t, NULL));
}

TEST_CASE("appendix B: Example 5 fetches two components in order", "[appendix_b]")
{
    esp_suit_ota_transport_t t;
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, run_example(5, 4096, &t, &h));
    TEST_ASSERT_EQUAL(0, t.component);
    esp_suit_ota_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_get_info(h, &info));
    TEST_ASSERT_EQUAL(2, info.num_components);
    TEST_ASSERT_EQUAL(2, info.num_pending);
    esp_suit_ota_abort(h);
}

TEST_CASE("appendix B: a URI component on a stream has no transport", "[appendix_b]")
{
    test_reset_state();
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_begin(&g_cfg, NULL, &h));
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_NO_TRANSPORT, feed_all(h, suit_vectors[1].data, suit_vectors[1].len, 4096));
    esp_suit_ota_abort(h);
}
