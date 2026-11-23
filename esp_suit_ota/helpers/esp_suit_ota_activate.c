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
        if (c->state != SUIT_COMP_STAGED || !c->target.copy_to_active) {
            continue;
        }
        const esp_partition_t *live = c->target.active;
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
    bool flipped = false;
    const esp_suit_ota_slots_ops_t *ops = ctx->cfg.slots_ops;   /* read on the out: path too */
    if (ctx->st != SUIT_ST_COMMITTED) {
        err = ESP_ERR_SUIT_OTA_STATE;
        goto out;
    }

    err = finalize_copies(ctx);
    if (err != ESP_OK) {
        goto out;
    }

    esp_suit_ota_flip_t flips[SUIT_MAX_COMP];
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
            /* the app target is listed for the set check; otadata, not the record, selects its slot */
            flips[n].name = c->target.slots;
            flips[n].new_index = c->target.slot_index;
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
            flips[n].name = c->target.slots;
            flips[n].new_index = c->target.active_index;
            n++;
        }
    }
    uint8_t app_slot = (app_staging != NULL) ? ctx->app_slot : ESP_SUIT_OTA_APP_SLOT_NONE;
    /* plain partitions and sinks own no target, and a configuration without a backend flips nothing */
    if (ops != NULL && ops->commit != NULL && (n > 0 || app_staging != NULL)) {
        err = ops->commit(ops->user, flips, n, app_slot, ctx->campaign_id);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "the slots backend refused the flip: %s", esp_err_to_name(err));
            if (err == ESP_ERR_INVALID_STATE) {
                err = ESP_ERR_SUIT_OTA_SLOTS;
            }
            goto out;
        }
        flipped = true;
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
            goto out;
        }
    }
    ctx->has_app = (app_staging != NULL);
    if (flipped && ops->activated != NULL) {
        esp_err_t done = ops->activated(ops->user);   /* the release is live and waits for a verdict */
        if (done != ESP_OK) {
            /*
             * The boot partition is already set, so the release goes live either way. The record
             * stays at activating, and the next boot reads that as an interrupted activation and
             * puts the data pairs back. Report it rather than hide it.
             */
            ESP_LOGE(TAG, "the slots backend did not record the activation: %s", esp_err_to_name(done));
        }
    }
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
    return ESP_OK;            /* the handle stays yours: end() releases it */
out:
    if (err != ESP_ERR_SUIT_OTA_STATE) {
        if (flipped && ops->revert != NULL) {
            ops->revert(ops->user);    /* undo the flip now, do not leave it for the next boot */
        }
        suit_post_abort(ctx, err, SUIT_ABORT_PHASE_ACTIVATE);
    }
    xSemaphoreGiveRecursive(ctx->lock);
    return err;
}
