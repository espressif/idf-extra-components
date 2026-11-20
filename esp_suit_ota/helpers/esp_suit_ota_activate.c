/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "suit_priv.h"
#include "esp_ota_ops.h"

static const char *TAG = SUIT_TAG;

static esp_err_t finalize_copies(struct esp_suit_ota_ctx *ctx)
{
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->state != SUIT_COMP_STAGED || c->target.slots[0] == '\0') {
            continue;
        }
        if (esp_ota_slots_get_kind(c->target.slots) != ESP_OTA_SLOTS_COPY) {
            continue;
        }
        const esp_partition_t *live = esp_ota_slots_get_active(c->target.slots);
        ESP_LOGI(TAG, "%s: copying %s to %s", c->id, c->target.staging->label, live->label);
        esp_err_t err = suit_sink_copy_partition(live, c->target.staging, c->size);
        if (err != ESP_OK) {
            return err;
        }
        uint8_t d[SUIT_DIGEST_LEN];
        err = suit_stream_digest_of_target(ctx, i, false, d);
        if (err != ESP_OK || memcmp(d, c->digest, SUIT_DIGEST_LEN) != 0) {
            ESP_LOGE(TAG, "%s: copy verification failed", c->id);
            return ESP_ERR_SUIT_OTA_DIGEST;
        }
    }
    return ESP_OK;
}

static esp_err_t write_installed(struct esp_suit_ota_ctx *ctx)
{
    suit_installed_t *r = calloc(1, sizeof(*r));
    if (r == NULL) {
        return ESP_ERR_NO_MEM;
    }
    r->sequence_number = ctx->man.sequence_number;
    r->num = (uint8_t)ctx->num_comp;
    strlcpy(r->description, ctx->description, sizeof(r->description));
    for (size_t i = 0; i < ctx->num_comp; i++) {
        strlcpy(r->comp[i].id, ctx->comp[i].id, sizeof(r->comp[i].id));
        memcpy(r->comp[i].digest, ctx->comp[i].digest, SUIT_DIGEST_LEN);
    }
    esp_err_t err = suit_persist_installed_write(r);
    free(r);
    if (err == ESP_OK && ctx->t != NULL) {
        suit_persist_etag_write(ctx->t->manifest_etag);
    }
    return err;
}

esp_err_t esp_suit_ota_activate(esp_suit_ota_handle_t h, bool *restart_needed)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (restart_needed != NULL) {
        *restart_needed = false;
    }
    struct esp_suit_ota_ctx *ctx = h;
    xSemaphoreTakeRecursive(ctx->lock, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    if (ctx->st != SUIT_ST_COMMITTED) {
        err = ESP_ERR_SUIT_OTA_STATE;
        goto out;
    }
    suit_journal_save(ctx, SUIT_PHASE_ACTIVATING);

    err = finalize_copies(ctx);
    if (err != ESP_OK) {
        goto out;
    }

    const char *names[SUIT_MAX_COMP];
    uint8_t idx[SUIT_MAX_COMP];
    size_t n = 0;
    const esp_partition_t *app_staging = NULL;
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->state != SUIT_COMP_STAGED) {
            continue;
        }
        if (c->target.is_app) {
            app_staging = c->target.staging;
        }
        if (c->target.slots[0] != '\0') {
            /* the app pair is listed for the group check; otadata, not the record, selects its slot */
            names[n] = c->target.slots;
            idx[n] = c->target.slot_index;
            n++;
        }
    }
    /*
     * A pair the manifest lists but skipped, because its live slot already holds the image, is part
     * of the release too: listing it at its current index keeps it where it is and lets the group
     * check see every member. Without it, an app-only change in a group with an unchanged data pair
     * would be refused here after the whole download.
     */
    for (size_t i = 0; n > 0 && i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->state == SUIT_COMP_INSTALLED && c->target.slots[0] != '\0') {
            names[n] = c->target.slots;
            idx[n] = esp_ota_slots_get_active_index(c->target.slots);
            n++;
        }
    }
    uint8_t app_slot = (app_staging != NULL) ? ctx->app_slot : ESP_OTA_SLOTS_APP_SLOT_NONE;
    if (n > 0 || app_staging != NULL) {       /* a map of plain partitions and sinks owns no pair */
        err = esp_ota_slots_commit_flip(names, idx, n, app_slot, ctx->campaign_id);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "slots flip failed: %s", esp_err_to_name(err));
            goto out;
        }
    }

    err = write_installed(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "installed record not written: %s", esp_err_to_name(err));
        goto out;
    }

    if (app_staging != NULL) {
        err = esp_ota_set_boot_partition(app_staging);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            suit_persist_add_rejected(ctx->manifest_digest);
            esp_ota_slots_revert();
            goto out;
        }
    }
    ctx->has_app = (app_staging != NULL);
    suit_journal_save(ctx, SUIT_PHASE_ACTIVATED);
    esp_suit_ota_event_manifest_t ev = {
        .sequence_number = ctx->man.sequence_number,
        .num_components = (uint8_t)ctx->num_comp,
    };
    strlcpy(ev.description, ctx->description, sizeof(ev.description));
    suit_post_event(ESP_SUIT_OTA_EVENT_ACTIVATED, &ev, sizeof(ev));
    ESP_LOGI(TAG, "release %llu activated%s", (unsigned long long)ctx->man.sequence_number,
             app_staging ? ", restart to run the new app" : "");
    suit_close_getters(ctx->t);
    if (restart_needed != NULL) {
        *restart_needed = (app_staging != NULL);
    }
    xSemaphoreGiveRecursive(ctx->lock);
    return ESP_OK;            /* the handle stays yours: end() or abort() releases it */
out:
    if (err != ESP_ERR_SUIT_OTA_STATE) {
        suit_journal_save(ctx, SUIT_PHASE_READY);
        suit_post_abort(ctx, err, SUIT_PHASE_ACTIVATING);
    }
    xSemaphoreGiveRecursive(ctx->lock);
    return err;
}

esp_err_t esp_suit_ota_confirm(void)
{
    esp_ota_img_states_t st;
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_mark_app_valid_cancel_rollback failed: %s", esp_err_to_name(err));
            return err;
        }
    }
    esp_err_t err = esp_ota_slots_confirm();
    if (err != ESP_OK && err != ESP_ERR_OTA_SLOTS_STATE) {
        return err;                           /* STATE: no pair was ever declared, nothing to confirm */
    }
    suit_journal_t j;
    if (suit_persist_journal_read(&j) == ESP_OK && j.phase == SUIT_PHASE_ACTIVATED) {
        suit_persist_journal_erase();
        ESP_LOGI(TAG, "release %llu confirmed", (unsigned long long)j.sequence_number);
    }
    return ESP_OK;
}

esp_err_t esp_suit_ota_revert(void)
{
    suit_journal_t j;
    if (suit_persist_journal_read(&j) != ESP_OK || j.phase != SUIT_PHASE_ACTIVATED) {
        return ESP_ERR_SUIT_OTA_STATE;
    }
    if (j.app_slot != ESP_OTA_SLOTS_APP_SLOT_NONE) {
        ESP_LOGE(TAG, "revert is for data-only releases; use esp_ota_mark_app_invalid_rollback_and_reboot()");
        return ESP_ERR_SUIT_OTA_STATE;
    }
    esp_ota_slots_state_t st;
    uint32_t cid = 0;
    esp_err_t err = esp_ota_slots_get_state(&st, &cid, NULL);
    if (err != ESP_OK) {
        return err;                 /* esp_ota_slots_init() first: only the slot record knows what to undo */
    }
    if (st == ESP_OTA_SLOTS_PENDING && cid == j.campaign_id) {
        err = esp_ota_slots_revert();
        if (err != ESP_OK) {
            return err;
        }
    }                               /* otherwise this release flipped no pair: nothing to undo, only to remember */
    suit_persist_add_rejected(j.manifest_digest);
    suit_persist_journal_erase();
    ESP_LOGW(TAG, "release %llu reverted and recorded as rejected", (unsigned long long)j.sequence_number);
    return ESP_OK;
}
