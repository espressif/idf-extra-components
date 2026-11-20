/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* The slot manager and the component's NVS records, on the device's own flash. */
#include <string.h>
#include "unity.h"
#include "nvs.h"
#include "esp_ota_slots.h"
#include "suit_priv.h"

/* the slots record is one NVS blob: namespace "ota_slots", key "rec" */
static void slots_record_erase(void)
{
    nvs_handle_t h;
    if (nvs_open("ota_slots", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void slots_record_damage(void)
{
    nvs_handle_t h;
    uint8_t junk[64];
    memset(junk, 0xa5, sizeof(junk));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("ota_slots", NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_blob(h, "rec", junk, sizeof(junk)));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(h));
    nvs_close(h);
}

TEST_CASE("slots: AB pair flips, survives re-init, confirms and reverts", "[slots]")
{
    slots_record_erase();
    static const esp_ota_slots_decl_t decls[] = {
        { .name = "f1",  .kind = ESP_OTA_SLOTS_AB,   .primary = "f1_0", .secondary = "f1_1", .group = "g" },
        { .name = "cfg", .kind = ESP_OTA_SLOTS_COPY, .primary = "cfg",  .secondary = "cfg_staging", .group = "g" },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(decls, 2));
    TEST_ASSERT_EQUAL(ESP_OTA_SLOTS_AB, esp_ota_slots_get_kind("f1"));
    TEST_ASSERT_EQUAL(ESP_OTA_SLOTS_COPY, esp_ota_slots_get_kind("cfg"));
    TEST_ASSERT_EQUAL_STRING("f1_0", esp_ota_slots_get_active("f1")->label);
    TEST_ASSERT_EQUAL_STRING("f1_1", esp_ota_slots_get_staging("f1")->label);
    TEST_ASSERT_EQUAL_STRING("cfg_staging", esp_ota_slots_get_staging("cfg")->label);

    const char *only_f1[] = { "f1" };
    uint8_t idx1[] = { 1 };
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_PARTIAL_GROUP, esp_ota_slots_commit_flip(only_f1, idx1, 1, 0xFF, 7));
    const char *both[] = { "f1", "cfg" };
    uint8_t idx[] = { 1, 0 };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_commit_flip(both, idx, 2, ESP_OTA_SLOTS_APP_SLOT_NONE, 7));
    TEST_ASSERT_EQUAL_STRING("f1_1", esp_ota_slots_get_active("f1")->label);
    esp_ota_slots_state_t st;
    uint32_t cid;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_get_state(&st, &cid, NULL));
    TEST_ASSERT_EQUAL(ESP_OTA_SLOTS_PENDING, st);
    TEST_ASSERT_EQUAL(7, cid);

    /* a data-only pending record survives a re-init (reboot) */
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(decls, 2));
    TEST_ASSERT_EQUAL_STRING("f1_1", esp_ota_slots_get_active("f1")->label);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_get_state(&st, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OTA_SLOTS_PENDING, st);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_revert());
    TEST_ASSERT_EQUAL_STRING("f1_0", esp_ota_slots_get_active("f1")->label);

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_commit_flip(both, idx, 2, ESP_OTA_SLOTS_APP_SLOT_NONE, 8));
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_confirm());
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_STATE, esp_ota_slots_revert());
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(decls, 2));
    TEST_ASSERT_EQUAL_STRING("f1_1", esp_ota_slots_get_active("f1")->label);

    /* a damaged record is reported, and the pairs fall back to index 0 until acknowledged */
    slots_record_damage();
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(decls, 2));
    TEST_ASSERT_TRUE(esp_ota_slots_is_degraded());
    TEST_ASSERT_EQUAL_STRING("f1_0", esp_ota_slots_get_active("f1")->label);
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_DEGRADED, esp_ota_slots_commit_flip(both, idx, 2, 0xFF, 9));
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_ack_degraded());
    TEST_ASSERT_FALSE(esp_ota_slots_is_degraded());
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_commit_flip(both, idx, 2, 0xFF, 9));

    /* with the record gone it is a first boot, not a damaged one */
    slots_record_erase();
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(decls, 2));
    TEST_ASSERT_FALSE(esp_ota_slots_is_degraded());
    TEST_ASSERT_EQUAL_STRING("f1_0", esp_ota_slots_get_active("f1")->label);
}

TEST_CASE("slots: declarations are complete or refused", "[slots]")
{
    /* a single pair may name its partition */
    static const esp_ota_slots_decl_t single[] = {
        { .name = "blob", .kind = ESP_OTA_SLOTS_SINGLE, .primary = "blob" },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(single, 1));
    TEST_ASSERT_EQUAL(ESP_OTA_SLOTS_SINGLE, esp_ota_slots_get_kind("blob"));
    TEST_ASSERT_EQUAL_STRING("blob", esp_ota_slots_get_staging("blob")->label);
    TEST_ASSERT_EQUAL_STRING("blob", esp_ota_slots_get_active("blob")->label);

    /* a single pair cannot join a group */
    static const esp_ota_slots_decl_t grouped[] = {
        { .name = "blob", .kind = ESP_OTA_SLOTS_SINGLE, .primary = "blob", .group = "g" },
    };
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_NOT_POWERSAFE, esp_ota_slots_init(grouped, 1));

    /* a declaration without a kind is refused */
    static const esp_ota_slots_decl_t no_kind[] = { { .name = "f1", .primary = "f1_0", .secondary = "f1_1" } };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_slots_init(no_kind, 1));

    /* naming one partition of a pair without the other is refused */
    static const esp_ota_slots_decl_t half[] = { { .name = "f1", .kind = ESP_OTA_SLOTS_AB, .primary = "f1_0" } };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_slots_init(half, 1));
    static const esp_ota_slots_decl_t half2[] = { { .name = "f1", .kind = ESP_OTA_SLOTS_AB, .secondary = "f1_1" } };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_slots_init(half2, 1));

    /* a single pair must not name a second partition */
    static const esp_ota_slots_decl_t extra[] = {
        { .name = "blob", .kind = ESP_OTA_SLOTS_SINGLE, .primary = "blob", .secondary = "f1_0" },
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_slots_init(extra, 1));

    /* a label that is not in the partition table */
    static const esp_ota_slots_decl_t missing[] = {
        { .name = "f1", .kind = ESP_OTA_SLOTS_AB, .primary = "f1_0", .secondary = "nope" },
    };
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_NOT_FOUND, esp_ota_slots_init(missing, 1));

    /* the labels are free: any partition can be any slot */
    static const esp_ota_slots_decl_t swapped[] = {
        { .name = "data", .kind = ESP_OTA_SLOTS_AB, .primary = "cfg", .secondary = "blob" },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(swapped, 1));
    TEST_ASSERT_EQUAL_STRING("cfg", esp_ota_slots_get_active("data")->label);
    TEST_ASSERT_EQUAL_STRING("blob", esp_ota_slots_get_staging("data")->label);
}

TEST_CASE("slots: a pair that names no partition derives its labels", "[slots]")
{
    slots_record_erase();                             /* an earlier case left f1 flipped */
    /* AB takes name_0 and name_1, COPY takes name and name_staging, SINGLE takes name */
    static const esp_ota_slots_decl_t derived[] = {
        { .name = "f1",   .kind = ESP_OTA_SLOTS_AB },
        { .name = "cfg",  .kind = ESP_OTA_SLOTS_COPY },
        { .name = "blob", .kind = ESP_OTA_SLOTS_SINGLE },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_slots_init(derived, 3));
    TEST_ASSERT_EQUAL_STRING("f1_0", esp_ota_slots_get_active("f1")->label);
    TEST_ASSERT_EQUAL_STRING("f1_1", esp_ota_slots_get_staging("f1")->label);
    TEST_ASSERT_EQUAL_STRING("cfg", esp_ota_slots_get_active("cfg")->label);
    TEST_ASSERT_EQUAL_STRING("cfg_staging", esp_ota_slots_get_staging("cfg")->label);
    TEST_ASSERT_EQUAL_STRING("blob", esp_ota_slots_get_active("blob")->label);
    TEST_ASSERT_EQUAL_STRING("blob", esp_ota_slots_get_staging("blob")->label);

    /* a derived label that no partition carries */
    static const esp_ota_slots_decl_t nothing[] = { { .name = "zz", .kind = ESP_OTA_SLOTS_AB } };
    TEST_ASSERT_EQUAL(ESP_ERR_OTA_SLOTS_NOT_FOUND, esp_ota_slots_init(nothing, 1));

    /* a name too long for the derived label, which a named pair would accept */
    static const esp_ota_slots_decl_t too_long[] = { { .name = "configuration", .kind = ESP_OTA_SLOTS_COPY } };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_slots_init(too_long, 1));
}

TEST_CASE("persist: journal and rejected ring", "[persist]")
{
    suit_journal_t j;
    memset(&j, 0, sizeof(j));
    j.magic = 0x4A535355u;
    j.phase = SUIT_PHASE_STAGING;
    j.bytes_written = 12345;
    j.manifest_digest[0] = 0xAB;
    TEST_ASSERT_EQUAL(ESP_OK, suit_persist_journal_write(&j));
    suit_journal_t r;
    TEST_ASSERT_EQUAL(ESP_OK, suit_persist_journal_read(&r));
    TEST_ASSERT_EQUAL(12345, r.bytes_written);
    TEST_ASSERT_EQUAL(ESP_OK, suit_persist_journal_erase());
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, suit_persist_journal_read(&r));

    uint8_t d[32] = { 1 };
    TEST_ASSERT_FALSE(suit_persist_is_rejected(d));
    TEST_ASSERT_EQUAL(ESP_OK, suit_persist_add_rejected(d));
    TEST_ASSERT_TRUE(suit_persist_is_rejected(d));
    for (int i = 0; i < 9; i++) {
        uint8_t e[32] = { 0x10 + i };
        suit_persist_add_rejected(e);
    }
    TEST_ASSERT_FALSE(suit_persist_is_rejected(d));   /* pushed out of the ring of 8 */
}
