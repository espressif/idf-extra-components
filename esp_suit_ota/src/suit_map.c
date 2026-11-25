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

/* the slots backend owns slot selection; without one, only cfg.map can name a target */
static esp_err_t resolve_slots(struct esp_suit_ota_ctx *ctx, suit_target_t *tg, const char *name)
{
    const esp_suit_ota_slots_ops_t *ops = ctx->cfg.slots_ops;
    if (ops == NULL || ops->resolve == NULL) {
        ESP_LOGE(TAG, "'%s' needs a slots backend: set cfg.slots_ops, or name a partition in cfg.map", name);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    esp_suit_ota_target_t t;
    memset(&t, 0, sizeof(t));
    esp_err_t err = ops->resolve(ops->user, name, &t);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "the slots backend is not usable, so '%s' cannot be resolved", name);
        return ESP_ERR_SUIT_OTA_SLOTS;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "the slots backend knows no target named '%s'", name);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    if (t.staging == NULL || t.active == NULL) {
        ESP_LOGE(TAG, "the slots backend gave no partitions for '%s'", name);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    tg->staging = t.staging;
    tg->active = t.active;
    tg->is_app = t.is_app;
    tg->slot_index = t.slot_index;
    tg->active_index = t.active_index;
    tg->copy_to_active = t.copy_to_active;
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
            err = resolve_slots(ctx, tg, e->slots);
            if (err != ESP_OK) {
                return err;
            }
        } else if (e->target_partition != NULL) {
            tg->staging = e->target_partition;
            tg->is_app = (e->target_partition->type == ESP_PARTITION_TYPE_APP);
            if (tg->is_app) {
                if (e->live_partition != NULL || e->copy_to_live) {
                    ESP_LOGE(TAG, "%s: live_partition and copy_to_live are for data targets", c->id);
                    return ESP_ERR_SUIT_OTA_MAP;
                }
                tg->slot_index = (uint8_t)(e->target_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
                tg->active = esp_ota_get_running_partition();
            } else if (e->live_partition != NULL) {
                if (e->copy_to_live && e->live_partition == e->target_partition) {
                    ESP_LOGE(TAG, "%s: copy_to_live needs two different partitions", c->id);
                    return ESP_ERR_SUIT_OTA_MAP;
                }
                tg->active = e->live_partition;      /* a separate partition holds the installed image */
                tg->copy_to_active = e->copy_to_live;
            } else if (e->copy_to_live) {
                ESP_LOGE(TAG, "%s: copy_to_live needs live_partition", c->id);
                return ESP_ERR_SUIT_OTA_MAP;
            } else {
                tg->active = e->target_partition;    /* written in place: the live image is the staged one */
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
    char name[ESP_SUIT_OTA_SLOT_NAME_MAX + 1];
    if (last->len == 0 || last->len > ESP_SUIT_OTA_SLOT_NAME_MAX) {
        ESP_LOGE(TAG, "component %s has no map entry and its name does not fit a target name", c->id);
        return ESP_ERR_SUIT_OTA_MAP;
    }
    memcpy(name, last->ptr, last->len);
    name[last->len] = '\0';
    if (strcmp(name, "bootloader") == 0) {
        ESP_LOGE(TAG, "the component name 'bootloader' is reserved");
        return ESP_ERR_SUIT_OTA_MAP;
    }
    err = resolve_slots(ctx, tg, name);
    if (err != ESP_OK) {
        return err;
    }
    tg->verify = tg->is_app ? ESP_SUIT_VERIFY_APP_IMAGE : ESP_SUIT_VERIFY_DIGEST;
    return ESP_OK;
}

esp_err_t suit_map_check_groups(struct esp_suit_ota_ctx *ctx)
{
    const esp_suit_ota_slots_ops_t *ops = ctx->cfg.slots_ops;
    if (ops == NULL || ops->check_set == NULL) {
        return ESP_OK;                       /* no backend, or a backend without sets */
    }
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
    esp_err_t err = ops->check_set(ops->user, names, n);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "the slots backend is not usable, so the group check cannot run");
        return ESP_ERR_SUIT_OTA_SLOTS;
    }
    return err == ESP_OK ? ESP_OK : ESP_ERR_SUIT_OTA_GROUP;
}
