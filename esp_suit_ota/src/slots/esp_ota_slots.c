/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_crc.h"
#include "esp_ota_slots.h"
#include "nvs.h"
#include "esp_ota_ops.h"

/*
 * This file knows nothing about SUIT, so it can move to app_update unchanged.
 *
 * The record is one NVS blob, namespace "ota_slots", key "rec", guarded by a magic value, a
 * layout version and a CRC. NVS makes the write itself safe against power loss: a blob either
 * lands whole or the previous one stays, so the record needs no second copy of its own.
 */

static const char *TAG = "esp_ota_slots";

#define RECORD_MAGIC    0x534C4F54u   /* 'TOLS' */
#define RECORD_VERSION  3
#define RECORD_NS       "ota_slots"
#define RECORD_KEY      "rec"
#define LABEL_CAP       17            /* a partition label is 16 characters plus the terminator */
#define PARTS_PER_PAIR  2             /* AB: slot 0 and 1; COPY: live and staging; SINGLE uses the first */
#define AB_SLOT_COUNT   2
#define APP_SLOT_NOT_OTA 0xFE        /* the running app is not in an OTA slot, e.g. factory */

typedef struct {
    char name[ESP_OTA_SLOTS_NAME_MAX + 1];
    uint8_t active;
    uint8_t prev;
    uint8_t pad[2];                   /* keeps the entry a multiple of 4 bytes */
} record_entry_t;

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t state;
    uint8_t num;
    uint8_t expected_app_slot;
    uint32_t campaign_id;
    record_entry_t entry[ESP_OTA_SLOTS_MAX_PAIRS];
    uint32_t crc;
} record_t;

typedef struct {
    esp_ota_slots_decl_t decl;        /* the declaration, with primary and secondary filled in by resolve_decl() */
    const esp_partition_t *part[PARTS_PER_PAIR];   /* AB: slot 0 and slot 1; COPY: live and staging; SINGLE and APP: see below */
} pair_t;

static pair_t s_pairs[ESP_OTA_SLOTS_MAX_PAIRS];
static size_t s_num_pairs;
static record_t s_rec;
static bool s_inited;
static bool s_degraded;
static bool s_degraded_acked;

static uint32_t record_crc(const record_t *r)
{
    return esp_crc32_le(0, (const uint8_t *)r, offsetof(record_t, crc));
}

static pair_t *find_pair(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < s_num_pairs; i++) {
        if (strcmp(s_pairs[i].decl.name, name) == 0) {
            return &s_pairs[i];
        }
    }
    return NULL;
}

static record_entry_t *find_entry(const char *name)
{
    for (size_t i = 0; i < s_rec.num; i++) {
        if (strcmp(s_rec.entry[i].name, name) == 0) {
            return &s_rec.entry[i];
        }
    }
    return NULL;
}

static const esp_partition_t *find_part(const char *label)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label);
}

/*
 * Three outcomes, and the caller must tell them apart:
 *   ESP_OK             the record was read
 *   ESP_ERR_NOT_FOUND  no record yet, which is a first boot
 *   anything else      a record exists but cannot be trusted, or NVS is not usable
 */
static esp_err_t record_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(RECORD_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;            /* the namespace does not exist yet */
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open('%s') failed: %s", RECORD_NS, esp_err_to_name(err));
        return err;                          /* NVS not initialised, or out of pages */
    }
    size_t len = sizeof(s_rec);
    err = nvs_get_blob(h, RECORD_KEY, &s_rec, &len);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reading the slots record failed: %s", esp_err_to_name(err));
        return err;
    }
    if (len != sizeof(s_rec) || s_rec.magic != RECORD_MAGIC) {
        ESP_LOGE(TAG, "the slots record is not a record of this component");
        return ESP_ERR_INVALID_CRC;
    }
    if (s_rec.version != RECORD_VERSION) {
        ESP_LOGE(TAG, "slots record layout %u, this build writes %u", s_rec.version, RECORD_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    if (s_rec.num > ESP_OTA_SLOTS_MAX_PAIRS || s_rec.crc != record_crc(&s_rec)) {
        ESP_LOGE(TAG, "the slots record is damaged");
        return ESP_ERR_INVALID_CRC;
    }
    return ESP_OK;
}

static esp_err_t record_store(void)
{
    s_rec.magic = RECORD_MAGIC;
    s_rec.version = RECORD_VERSION;
    s_rec.crc = record_crc(&s_rec);
    nvs_handle_t h;
    esp_err_t err = nvs_open(RECORD_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open('%s') failed: %s", RECORD_NS, esp_err_to_name(err));
        return ESP_ERR_OTA_SLOTS_RECORD;
    }
    err = nvs_set_blob(h, RECORD_KEY, &s_rec, sizeof(s_rec));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "writing the slots record failed: %s", esp_err_to_name(err));
        return ESP_ERR_OTA_SLOTS_RECORD;
    }
    return ESP_OK;
}

static uint8_t running_app_slot(void)
{
    const esp_partition_t *p = esp_ota_get_running_partition();
    if (p != NULL && p->type == ESP_PARTITION_TYPE_APP && p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
            p->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
        return (uint8_t)(p->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    }
    return APP_SLOT_NOT_OTA;
}

/*
 * Resolve one declaration into a pair.
 *
 * The kind is always declared. The partition labels are optional: a pair that names neither
 * partition takes "<name>_0" and "<name>_1" for AB, "<name>" and "<name>_staging" for COPY,
 * and "<name>" for SINGLE. Naming one partition without the other is refused, because the
 * missing one would be guessed while the other was given. After resolution the pair holds
 * the labels it really uses, so nothing downstream has to repeat this.
 */
static esp_err_t derive_label(const char *name, const char *suffix, char *out)
{
    int len = snprintf(out, LABEL_CAP, "%s%s", name, suffix);
    if (len < 0 || len >= LABEL_CAP) {
        ESP_LOGE(TAG, "pair '%s': the label '%s%s' is longer than %d characters, name the partitions in the declaration",
                 name, name, suffix, LABEL_CAP - 1);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t resolve_decl(const esp_ota_slots_decl_t *d, pair_t *p)
{
    memset(p, 0, sizeof(*p));
    p->decl = *d;
    if (d->name == NULL || d->name[0] == '\0' || strlen(d->name) > ESP_OTA_SLOTS_NAME_MAX) {
        ESP_LOGE(TAG, "a pair name is missing or longer than %d characters", ESP_OTA_SLOTS_NAME_MAX);
        return ESP_ERR_INVALID_ARG;
    }
    if (d->kind == ESP_OTA_SLOTS_NONE) {
        ESP_LOGE(TAG, "pair '%s' has no kind: declare APP, AB, COPY or SINGLE", d->name);
        return ESP_ERR_INVALID_ARG;
    }

    if (d->kind == ESP_OTA_SLOTS_APP) {
        if (d->primary != NULL || d->secondary != NULL) {
            ESP_LOGE(TAG, "pair '%s' is the app pair: otadata selects its partitions, do not name them", d->name);
            return ESP_ERR_INVALID_ARG;
        }
        p->part[0] = esp_ota_get_running_partition();
        p->part[1] = esp_ota_get_next_update_partition(NULL);
        if (p->part[0] == NULL || p->part[1] == NULL || p->part[0] == p->part[1]) {
            ESP_LOGE(TAG, "pair '%s' needs two OTA app partitions", d->name);
            return ESP_ERR_OTA_SLOTS_NOT_FOUND;
        }
        p->decl.primary = p->part[0]->label;
        p->decl.secondary = p->part[1]->label;
        return ESP_OK;
    }

    char derived[PARTS_PER_PAIR][LABEL_CAP];
    const char *label[PARTS_PER_PAIR] = { d->primary, d->secondary };

    if (d->kind == ESP_OTA_SLOTS_SINGLE) {
        if (label[1] != NULL) {
            ESP_LOGE(TAG, "pair '%s' is single but names a second partition", d->name);
            return ESP_ERR_INVALID_ARG;
        }
        if (d->group != NULL) {
            ESP_LOGE(TAG, "pair '%s' is single and cannot be in group '%s'", d->name, d->group);
            return ESP_ERR_OTA_SLOTS_NOT_POWERSAFE;
        }
        if (label[0] == NULL && derive_label(d->name, "", derived[0]) != ESP_OK) {
            return ESP_ERR_INVALID_ARG;
        }
        label[0] = (label[0] != NULL) ? label[0] : derived[0];
    } else {
        if ((label[0] == NULL) != (label[1] == NULL)) {
            ESP_LOGE(TAG, "pair '%s' names one partition only: name both or neither", d->name);
            return ESP_ERR_INVALID_ARG;
        }
        if (label[0] == NULL) {
            const char *suffix[PARTS_PER_PAIR] = { "_0", "_1" };
            if (d->kind == ESP_OTA_SLOTS_COPY) {
                suffix[0] = "";
                suffix[1] = "_staging";
            }
            for (int i = 0; i < PARTS_PER_PAIR; i++) {
                if (derive_label(d->name, suffix[i], derived[i]) != ESP_OK) {
                    return ESP_ERR_INVALID_ARG;
                }
                label[i] = derived[i];
            }
        }
    }

    for (int i = 0; i < PARTS_PER_PAIR; i++) {
        if (label[i] == NULL) {
            continue;
        }
        p->part[i] = find_part(label[i]);
        if (p->part[i] == NULL) {
            ESP_LOGE(TAG, "pair '%s': no partition labelled '%s'", d->name, label[i]);
            return ESP_ERR_OTA_SLOTS_NOT_FOUND;
        }
    }
    if (p->part[1] != NULL && p->part[1] == p->part[0]) {
        ESP_LOGE(TAG, "pair '%s' names the same partition twice", d->name);
        return ESP_ERR_INVALID_ARG;
    }
    p->decl.primary = p->part[0]->label;
    p->decl.secondary = (p->part[1] != NULL) ? p->part[1]->label : NULL;
    return ESP_OK;
}

esp_err_t esp_ota_slots_init(const esp_ota_slots_decl_t *decls, size_t n)
{
    if (decls == NULL || n == 0 || n > ESP_OTA_SLOTS_MAX_PAIRS) {
        return ESP_ERR_INVALID_ARG;
    }
    s_inited = false;
    s_degraded = false;
    s_degraded_acked = false;
    s_num_pairs = 0;
    for (size_t i = 0; i < n; i++) {
        if (find_pair(decls[i].name) != NULL) {        /* searches the pairs registered so far */
            ESP_LOGE(TAG, "pair '%s' is declared twice", decls[i].name);
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = resolve_decl(&decls[i], &s_pairs[s_num_pairs]);
        if (err != ESP_OK) {
            return err;
        }
        s_num_pairs++;
    }
    esp_err_t err = record_load();
    if (err == ESP_ERR_NOT_FOUND) {
        memset(&s_rec, 0, sizeof(s_rec));
        s_rec.expected_app_slot = ESP_OTA_SLOTS_APP_SLOT_NONE;
        ESP_LOGI(TAG, "no slots record yet, every pair starts at index 0");
    } else if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGE(TAG, "call nvs_flash_init() before esp_ota_slots_init()");
        return ESP_ERR_OTA_SLOTS_RECORD;
    } else if (err != ESP_OK) {
        memset(&s_rec, 0, sizeof(s_rec));
        s_rec.expected_app_slot = ESP_OTA_SLOTS_APP_SLOT_NONE;
        s_degraded = true;
        ESP_LOGE(TAG, "the slots record is unusable; pairs derived at index 0 until acknowledged");
    }
    /* every declared pair has an entry */
    for (size_t i = 0; i < s_num_pairs; i++) {
        if (find_entry(s_pairs[i].decl.name) == NULL && s_rec.num < ESP_OTA_SLOTS_MAX_PAIRS) {
            record_entry_t *e = &s_rec.entry[s_rec.num++];
            memset(e, 0, sizeof(*e));
            strlcpy(e->name, s_pairs[i].decl.name, sizeof(e->name));
        }
    }
    s_inited = true;
    if (s_rec.state == ESP_OTA_SLOTS_PENDING && s_rec.expected_app_slot != ESP_OTA_SLOTS_APP_SLOT_NONE &&
            running_app_slot() != s_rec.expected_app_slot) {
        ESP_LOGW(TAG, "pending record for app slot %u but slot %u is running: previous data restored",
                 s_rec.expected_app_slot, running_app_slot());
        for (size_t i = 0; i < s_rec.num; i++) {
            s_rec.entry[i].active = s_rec.entry[i].prev;
        }
        s_rec.state = ESP_OTA_SLOTS_CONFIRMED;
        s_rec.expected_app_slot = ESP_OTA_SLOTS_APP_SLOT_NONE;
        if (!s_degraded) {
            record_store();
        }
    }
    return ESP_OK;
}

esp_ota_slots_kind_t esp_ota_slots_get_kind(const char *name)
{
    pair_t *p = s_inited ? find_pair(name) : NULL;
    return p ? p->decl.kind : ESP_OTA_SLOTS_NONE;
}
uint8_t esp_ota_slots_get_active_index(const char *name)
{
    pair_t *p = s_inited ? find_pair(name) : NULL;
    if (p == NULL || p->decl.kind != ESP_OTA_SLOTS_AB) {
        return 0;                        /* otadata owns the app selection; COPY and SINGLE have one slot */
    }
    record_entry_t *e = find_entry(name);
    return (e != NULL && e->active < AB_SLOT_COUNT) ? e->active : 0;
}

const esp_partition_t *esp_ota_slots_get_active(const char *name)
{
    pair_t *p = s_inited ? find_pair(name) : NULL;
    if (p == NULL) {
        return NULL;
    }
    if (p->decl.kind == ESP_OTA_SLOTS_AB) {
        return p->part[esp_ota_slots_get_active_index(name)];
    }
    return p->part[0];
}

const esp_partition_t *esp_ota_slots_get_staging(const char *name)
{
    pair_t *p = s_inited ? find_pair(name) : NULL;
    if (p == NULL) {
        return NULL;
    }
    switch (p->decl.kind) {
    case ESP_OTA_SLOTS_AB:
        return p->part[1 - esp_ota_slots_get_active_index(name)];
    case ESP_OTA_SLOTS_APP:
    case ESP_OTA_SLOTS_COPY:
        return p->part[1];
    case ESP_OTA_SLOTS_SINGLE:
        return p->part[0];
    default:
        return NULL;
    }
}

esp_err_t esp_ota_slots_check_group(const char *const *names, size_t n)
{
    if (n == 0) {
        return ESP_OK;
    }
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    for (size_t i = 0; i < n; i++) {
        pair_t *p = find_pair(names[i]);
        if (p == NULL || p->decl.group == NULL) {
            continue;
        }
        for (size_t k = 0; k < s_num_pairs; k++) {
            pair_t *m = &s_pairs[k];
            if (m->decl.group == NULL || strcmp(m->decl.group, p->decl.group) != 0) {
                continue;
            }
            bool listed = false;
            for (size_t j = 0; j < n && !listed; j++) {
                listed = (strcmp(names[j], m->decl.name) == 0);
            }
            if (!listed) {
                ESP_LOGE(TAG, "group '%s': pair '%s' is updated but '%s' is not", p->decl.group, names[i],
                         m->decl.name);
                return ESP_ERR_OTA_SLOTS_PARTIAL_GROUP;
            }
        }
    }
    return ESP_OK;
}

esp_err_t esp_ota_slots_commit_flip(const char *const *names, const uint8_t *new_index, size_t n,
                                    uint8_t expected_app_slot, uint32_t campaign_id)
{
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    if (s_degraded && !s_degraded_acked) {
        return ESP_ERR_OTA_SLOTS_DEGRADED;
    }
    esp_err_t err = esp_ota_slots_check_group(names, n);
    if (err != ESP_OK) {
        return err;
    }
    for (size_t i = 0; i < n; i++) {
        pair_t *p = find_pair(names[i]);
        if (p == NULL) {
            return ESP_ERR_OTA_SLOTS_NOT_FOUND;
        }
        if (p->decl.kind == ESP_OTA_SLOTS_AB && new_index[i] >= AB_SLOT_COUNT) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < s_rec.num; i++) {
        s_rec.entry[i].prev = s_rec.entry[i].active;
    }
    for (size_t i = 0; i < n; i++) {
        record_entry_t *e = find_entry(names[i]);
        pair_t *p = find_pair(names[i]);
        if (e != NULL && p->decl.kind == ESP_OTA_SLOTS_AB) {
            e->active = new_index[i];
        }
    }
    s_rec.state = ESP_OTA_SLOTS_PENDING;
    s_rec.expected_app_slot = expected_app_slot;
    s_rec.campaign_id = campaign_id;
    return record_store();
}

esp_err_t esp_ota_slots_get_state(esp_ota_slots_state_t *state, uint32_t *campaign_id, uint8_t *expected_app_slot)
{
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    if (state) {
        *state = (esp_ota_slots_state_t)s_rec.state;
    }
    if (campaign_id) {
        *campaign_id = s_rec.campaign_id;
    }
    if (expected_app_slot) {
        *expected_app_slot = s_rec.expected_app_slot;
    }
    return ESP_OK;
}

esp_err_t esp_ota_slots_confirm(void)
{
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    if (s_rec.state != ESP_OTA_SLOTS_PENDING) {
        return ESP_OK;
    }
    s_rec.state = ESP_OTA_SLOTS_CONFIRMED;
    s_rec.expected_app_slot = ESP_OTA_SLOTS_APP_SLOT_NONE;
    for (size_t i = 0; i < s_rec.num; i++) {
        s_rec.entry[i].prev = s_rec.entry[i].active;
    }
    return record_store();
}

esp_err_t esp_ota_slots_revert(void)
{
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    if (s_rec.state != ESP_OTA_SLOTS_PENDING) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    for (size_t i = 0; i < s_rec.num; i++) {
        s_rec.entry[i].active = s_rec.entry[i].prev;
    }
    s_rec.state = ESP_OTA_SLOTS_CONFIRMED;
    s_rec.expected_app_slot = ESP_OTA_SLOTS_APP_SLOT_NONE;
    return record_store();
}

bool esp_ota_slots_is_degraded(void)
{
    return s_degraded && !s_degraded_acked;
}

esp_err_t esp_ota_slots_ack_degraded(void)
{
    if (!s_inited) {
        return ESP_ERR_OTA_SLOTS_STATE;
    }
    s_degraded_acked = true;
    return record_store();
}
