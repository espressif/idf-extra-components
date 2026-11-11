/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * draft-ietf-suit-manifest-38 compliance. Every vector in compliance.bin breaks exactly one rule,
 * or none, and names the result the processor must give. gen_vectors.py builds them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "esp_partition.h"
#include "esp_suit_ota.h"
#include "esp_ota_slots.h"
#include "test_common.h"

static const char *err_name(esp_err_t e)
{
    const char *n = esp_suit_ota_err_to_name(e);
    return n ? n : esp_err_to_name(e);
}

/* run every table vector at one chunk size; report each mismatch, then fail once */
static void run_table(size_t chunk)
{
    int failures = 0, ran = 0;
    for (size_t i = 0; i < suit_cvecs_count; i++) {
        const suit_cvec_t *v = &suit_cvecs[i];
        if (v->flags & CVEC_MANUAL) {
            continue;
        }
        test_reset_state();
        esp_err_t got = run_cvec(v, chunk);
        ran++;
        if (got != v->expect) {
            printf("MISMATCH %-42s want %s, got %s  [%s]\n", v->name, err_name(v->expect), err_name(got), v->rule);
            failures++;
        }
    }
    printf("%d of %d vectors as expected at chunk size %u\n", ran - failures, ran, (unsigned)chunk);
    TEST_ASSERT_EQUAL_MESSAGE(0, failures, "vectors with an unexpected result, listed above");
}

TEST_CASE("compliance: every vector gives the result the draft requires", "[compliance]")
{
    run_table(64);
}

TEST_CASE("compliance: results do not depend on how the bytes are split", "[compliance]")
{
    run_table(1);
    run_table(7);
    run_table(4096);
}

TEST_CASE("compliance: the payload lands in the sink byte for byte", "[compliance]")
{
    test_reset_state();
    const suit_cvec_t *v = cvec_find("good_three_components");
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_cvec(v, 64));
    static const size_t sizes[3] = { 200, 120, 80 };
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(sizes[i], g_mem[i].size);
        TEST_ASSERT_EQUAL(1, g_mem[i].closes);
        for (size_t k = 0; k < sizes[i]; k++) {      /* the pattern gen_vectors.py writes */
            uint8_t want = (uint8_t)((i * 31 + k * 7 + (k >> 8)) & 0xff);
            TEST_ASSERT_EQUAL_HEX8(want, g_mem[i].buf[k]);
        }
    }
}

/* ---------- robustness ---------- */

TEST_CASE("robustness: every truncation of a valid envelope is refused", "[robustness]")
{
    static const char *const names[] = { "robust_small", "good_three_components", "good_severed_install" };
    for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
        const suit_cvec_t *v = cvec_find(names[n]);
        const uint8_t *data = cvec_data(v);
        for (size_t cut = 0; cut < v->len; cut++) {
            test_reset_state();
            esp_err_t err = run_campaign(&g_cfg, data, cut, 64, false);
            if (err == ESP_SUIT_OTA_READY || err == ESP_OK) {
                printf("%s cut at %u of %u was accepted\n", names[n], (unsigned)cut, (unsigned)v->len);
                TEST_FAIL();
            }
        }
    }
}

/*
 * Every byte of an envelope is covered by the signature, a digest, or the framing, so changing any
 * single byte must stop the campaign.
 */
TEST_CASE("robustness: no single changed byte is accepted", "[robustness]")
{
    const suit_cvec_t *v = cvec_find("robust_small");
    uint8_t *copy = malloc(v->len);
    TEST_ASSERT_NOT_NULL(copy);
    static const uint8_t flips[] = { 0x01, 0x80 };
    for (size_t f = 0; f < sizeof(flips); f++) {
        for (size_t i = 0; i < v->len; i++) {
            memcpy(copy, cvec_data(v), v->len);
            copy[i] ^= flips[f];
            test_reset_state();
            esp_err_t err = run_campaign(&g_cfg, copy, v->len, 64, false);
            if (err == ESP_SUIT_OTA_READY || err == ESP_OK) {
                printf("byte %u ^ 0x%02x was accepted\n", (unsigned)i, flips[f]);
                free(copy);
                TEST_FAIL();
            }
        }
    }
    free(copy);
}

/* ---------- rules that need device state ---------- */

/*
 * These use the partition pair "blob", reached by the last part of the identifier [host, blob]
 * with no map: unlike a RAM sink, its installed image can be read back and recognized.
 */
static const esp_ota_slots_decl_t s_blob_decl[] = { { .name = "blob", .kind = ESP_OTA_SLOTS_SINGLE } };

static esp_suit_ota_config_t pair_setup(void)
{
    test_reset_state();
    const esp_partition_t *blob = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "blob");
    TEST_ASSERT_NOT_NULL(blob);
    TEST_ASSERT_EQUAL(ESP_OK, esp_partition_erase_range(blob, 0, blob->size));
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(s_blob_decl, 1));
    esp_suit_ota_config_t cfg = g_cfg;
    cfg.map = NULL;
    cfg.map_len = 0;
    return cfg;
}

static esp_err_t run_pair(const esp_suit_ota_config_t *cfg, const char *name, esp_suit_ota_handle_t *out)
{
    const suit_cvec_t *v = cvec_find(name);
    return run_campaign_keep(cfg, cvec_data(v), v->len, 64, false, out);
}

TEST_CASE("setup: rollback protection by sequence number", "[setup]")
{
    esp_suit_ota_config_t cfg = pair_setup();
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_pair(&cfg, "sequence_10", &h));
    bool restart = true;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_activate(h, &restart));
    TEST_ASSERT_FALSE(restart);                  /* data only */
    esp_suit_ota_end(h);
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_confirm());
    /* section 6.1: a lower sequence number is refused before any command runs */
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_SEQUENCE, run_pair(&cfg, "sequence_9", NULL));
    /* the same release, its image already in place */
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_UP_TO_DATE, run_pair(&cfg, "sequence_10", NULL));
    /* a newer release is accepted, and its unchanged image is not fetched again */
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_pair(&cfg, "sequence_11", &h));
    esp_suit_ota_component_t c;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_get_component(h, 0, &c));
    TEST_ASSERT_EQUAL(ESP_SUIT_COMP_ALREADY_INSTALLED, c.state);
    esp_suit_ota_abort(h);
}

TEST_CASE("setup: a new campaign waits until the last release is confirmed", "[setup]")
{
    esp_suit_ota_config_t cfg = pair_setup();
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_pair(&cfg, "sequence_10", &h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_activate(h, NULL));
    esp_suit_ota_end(h);
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_STATE, esp_suit_ota_begin(&cfg, NULL, &h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_confirm());
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_begin(&cfg, NULL, &h));
    esp_suit_ota_abort(h);
}

TEST_CASE("setup: a reverted release is refused the next time", "[setup]")
{
    esp_suit_ota_config_t cfg = pair_setup();
    const suit_cvec_t *v = cvec_find("partition_pair");
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_campaign_keep(&cfg, cvec_data(v), v->len, 64, false, &h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_activate(h, NULL));
    esp_suit_ota_end(h);
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_revert());
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_REJECTED, run_campaign(&cfg, cvec_data(v), v->len, 64, false));
}

TEST_CASE("setup: a release that changed no partition can still be reverted", "[setup]")
{
    esp_suit_ota_config_t cfg = pair_setup();
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_pair(&cfg, "sequence_10", &h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_activate(h, NULL));
    esp_suit_ota_end(h);
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_confirm());
    /* sequence 11 carries the same image: it activates without flipping anything */
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_pair(&cfg, "sequence_11", &h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_activate(h, NULL));
    esp_suit_ota_end(h);
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_revert());
    TEST_ASSERT_EQUAL(ESP_ERR_SUIT_OTA_REJECTED, run_pair(&cfg, "sequence_11", NULL));
}

TEST_CASE("campaign: the default mapping writes a declared partition pair", "[campaign]")
{
    esp_suit_ota_config_t cfg = pair_setup();
    const suit_cvec_t *v = cvec_find("partition_pair");
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_campaign(&cfg, cvec_data(v), v->len, 64, false));
    uint8_t head[16];
    TEST_ASSERT_EQUAL(ESP_OK, esp_partition_read(esp_ota_slots_get_active("blob"), 0, head, sizeof(head)));
    for (size_t k = 0; k < sizeof(head); k++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)((4 * 31 + k * 7) & 0xff), head[k]);
    }
    /* the same release again: the image is already there, nothing is fetched */
    esp_suit_ota_handle_t h;
    TEST_ASSERT_EQUAL(ESP_SUIT_OTA_READY, run_campaign_keep(&cfg, cvec_data(v), v->len, 64, false, &h));
    esp_suit_ota_component_t c;
    TEST_ASSERT_EQUAL(ESP_OK, esp_suit_ota_get_component(h, 0, &c));
    TEST_ASSERT_EQUAL(ESP_SUIT_COMP_ALREADY_INSTALLED, c.state);
    esp_suit_ota_abort(h);
}
