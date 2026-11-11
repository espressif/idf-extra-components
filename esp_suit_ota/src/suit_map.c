/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ctype.h>
#include "suit_priv.h"
#include "esp_ota_ops.h"

static const char *TAG = SUIT_TAG;

bool suit_cid_equal(const esp_suit_ota_span_t *a, size_t na, const esp_suit_ota_span_t *b, size_t nb)
{
    if (na != nb) {
        return false;
    }
    for (size_t i = 0; i < na; i++) {
        if (a[i].len != b[i].len || memcmp(a[i].ptr, b[i].ptr, a[i].len) != 0) {
            return false;
        }
    }
    return true;
}

void suit_cid_to_string(const esp_suit_ota_span_t *parts, size_t n, char *out, size_t cap)
{
    size_t pos = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n && pos + 1 < cap; i++) {
        if (i > 0) {
            out[pos++] = '/';
        }
        for (size_t j = 0; j < parts[i].len && pos + 1 < cap; j++) {
            uint8_t ch = parts[i].ptr[j];
            out[pos++] = isprint(ch) ? (char)ch : '.';   /* identifiers are bytes: show only printable ASCII */
        }
    }
    out[pos] = '\0';
}

static esp_err_t resolve_slots(suit_target_t *tg, const char *name)
{
    esp_ota_slots_kind_t kind = esp_ota_slots_get_kind(name);
    if (kind == ESP_OTA_SLOTS_NONE) {
        ESP_LOGE(TAG, "no declared pair named '%s'", name);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    tg->staging = esp_ota_slots_get_staging(name);
    tg->active = esp_ota_slots_get_active(name);
    if (tg->staging == NULL || tg->active == NULL) {
        return ESP_ERR_SUIT_OTA_MAP;
    }
    tg->is_app = (kind == ESP_OTA_SLOTS_APP);
    if (tg->is_app) {
        tg->slot_index = (uint8_t)(tg->staging->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    } else {
        tg->slot_index = (kind == ESP_OTA_SLOTS_AB) ? (uint8_t)(1 - esp_ota_slots_get_active_index(name)) : 0;
    }
    strlcpy(tg->slots, name, sizeof(tg->slots));
    return ESP_OK;
}

esp_err_t suit_map_resolve(struct esp_suit_ota_ctx *ctx, size_t index)
{
    suit_component_t *c = &ctx->comp[index];
    suit_target_t *tg = &c->target;
    memset(tg, 0, sizeof(*tg));
    const esp_suit_ota_span_t *parts = ctx->man.cid_parts[index];
    size_t nparts = ctx->man.cid_num_parts[index];

    const esp_suit_ota_map_entry_t *e = NULL;
    for (size_t i = 0; i < ctx->cfg.map_len; i++) {
        const esp_suit_ota_map_entry_t *m = &ctx->cfg.map[i];
        if (suit_cid_equal(m->id.parts, m->id.num_parts, parts, nparts)) {
            e = m;
            break;
        }
    }
    esp_err_t err;
    if (e != NULL) {
        tg->entry = e;
        tg->verify = e->verify;
        tg->bulk_erase = e->bulk_erase;
        if (e->sink != NULL) {
            tg->sink = e->sink;
            return ESP_OK;
        }
        if (e->slots != NULL) {
            err = resolve_slots(tg, e->slots);
            if (err != ESP_OK) {
                return err;
            }
        } else if (e->partition != NULL) {
            tg->staging = e->partition;
            tg->is_app = (e->partition->type == ESP_PARTITION_TYPE_APP);
            if (tg->is_app) {
                tg->slot_index = (uint8_t)(e->partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
                tg->active = esp_ota_get_running_partition();
            }
        } else {
            ESP_LOGE(TAG, "map entry for %s names no target", c->id);
            return ESP_ERR_SUIT_OTA_MAP;
        }
        if (tg->is_app && tg->verify == ESP_SUIT_VERIFY_DIGEST) {
            tg->verify = ESP_SUIT_VERIFY_APP_IMAGE;
        }
        return ESP_OK;
    }

    /* default mapping: the last identifier part names the pair */
    const esp_suit_ota_span_t *last = &parts[nparts - 1];
    char name[ESP_OTA_SLOTS_NAME_MAX + 1];
    if (last->len == 0 || last->len > ESP_OTA_SLOTS_NAME_MAX) {
        ESP_LOGE(TAG, "component %s has no map entry and its name does not fit a pair name", c->id);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    memcpy(name, last->ptr, last->len);
    name[last->len] = '\0';
    if (strcmp(name, "bootloader") == 0) {
        ESP_LOGE(TAG, "the component name 'bootloader' is reserved");
        return ESP_ERR_SUIT_OTA_MAP;
    }
    err = resolve_slots(tg, name);
    if (err != ESP_OK) {
        return err;
    }
    tg->verify = tg->is_app ? ESP_SUIT_VERIFY_APP_IMAGE : ESP_SUIT_VERIFY_DIGEST;
    return ESP_OK;
}

esp_err_t suit_map_check_groups(struct esp_suit_ota_ctx *ctx)
{
    const char *names[SUIT_MAX_COMP];
    size_t n = 0;
    for (size_t i = 0; i < ctx->num_comp; i++) {
        if (ctx->comp[i].target.slots[0] != '\0') {
            names[n++] = ctx->comp[i].target.slots;
        }
    }
    if (n == 0) {
        return ESP_OK;
    }
    esp_err_t err = esp_ota_slots_check_group(names, n);
    return err == ESP_OK ? ESP_OK : ESP_ERR_SUIT_OTA_GROUP;
}
