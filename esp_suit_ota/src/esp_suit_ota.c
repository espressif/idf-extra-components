/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <ctype.h>
#include "suit_priv.h"
#include "esp_ota_ops.h"

static const char *TAG = SUIT_TAG;

ESP_EVENT_DEFINE_BASE(ESP_SUIT_OTA_EVENT);

#define LOCK(ctx)    xSemaphoreTakeRecursive((ctx)->lock, portMAX_DELAY)
#define UNLOCK(ctx)  xSemaphoreGiveRecursive((ctx)->lock)

/* ---------- events ---------- */

void suit_post_event(esp_suit_ota_event_id_t id, const void *data, size_t len)
{
    TickType_t ticks = CONFIG_ESP_SUIT_OTA_EVENT_POST_TIMEOUT < 0 ? portMAX_DELAY
                       : pdMS_TO_TICKS(CONFIG_ESP_SUIT_OTA_EVENT_POST_TIMEOUT);
    esp_err_t err = esp_event_post(ESP_SUIT_OTA_EVENT, id, data, len, ticks);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "event %d not posted: %s", (int)id, esp_err_to_name(err));
    }
}

void suit_post_abort(struct esp_suit_ota_ctx *ctx, esp_err_t err, uint8_t phase)
{
    esp_suit_ota_event_abort_t ev = {
        .err = err,
        .phase = phase,
        .component = ctx->current == SIZE_MAX ? SUIT_COMPONENT_NONE : (uint8_t)ctx->current,
    };
    suit_post_event(ESP_SUIT_OTA_EVENT_ABORTED, &ev, sizeof(ev));
}

/* ---------- transport ---------- */

/* the scheme of a URI, or 0 when it has none */
static size_t scheme_len(const char *url)
{
    size_t i = 0;
    while (url[i] != '\0' && url[i] != ':' && url[i] != '/' && url[i] != '#' && url[i] != '?') {
        i++;
    }
    return url[i] == ':' ? i : 0;
}

/* the URI scheme, n characters long, is this one (any case) */
static bool scheme_is(const char *url, size_t n, const char *scheme)
{
    return n == strlen(scheme) && strncasecmp(url, scheme, n) == 0;
}

/* which registered getter a URI names */
static esp_suit_ota_mode_t mode_of(const char *url)
{
    size_t n = scheme_len(url);
    if (scheme_is(url, n, "https") || scheme_is(url, n, "http")) {
        return ESP_SUIT_MODE_HTTPS;
    }
    if (scheme_is(url, n, "ble")) {
        return ESP_SUIT_MODE_BLE;
    }
    if (scheme_is(url, n, "mqtt") || scheme_is(url, n, "mqtts")) {
        return ESP_SUIT_MODE_MQTT;
    }
    return ESP_SUIT_MODE_CUSTOM;
}

static esp_suit_ota_getter_t *getter_for(esp_suit_ota_transport_t *t, esp_suit_ota_mode_t mode)
{
    switch (mode) {
    case ESP_SUIT_MODE_BLE:    return &t->ble;
    case ESP_SUIT_MODE_MQTT:   return &t->mqtt;
    case ESP_SUIT_MODE_CUSTOM: return &t->custom;
    default:                   return &t->https;
    }
}

int esp_suit_ota_transport_get(esp_suit_ota_transport_t *t, uint8_t *buf, size_t cap)
{
    if (t == NULL) {
        return -ESP_ERR_SUIT_OTA_NO_TRANSPORT;
    }
    if (t->done) {
        return 0;
    }
    esp_suit_ota_getter_t *g = getter_for(t, t->mode);
    if (g->get == NULL) {
        return -ESP_ERR_SUIT_OTA_NO_TRANSPORT;
    }
    return g->get(t, &g->state, buf, cap);
}

void suit_close_getters(esp_suit_ota_transport_t *t)
{
    if (t == NULL) {
        return;
    }
    esp_suit_ota_getter_t *all[] = { &t->https, &t->ble, &t->mqtt, &t->custom };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (all[i]->close != NULL && all[i]->state != NULL) {
            all[i]->close(all[i]->state);
            all[i]->state = NULL;
        }
    }
}

/* RFC 3986 section 5.2, the two forms a release server uses: absolute, or relative to the base */
static esp_err_t resolve_uri(struct esp_suit_ota_ctx *ctx, const char *uri, char *out, size_t cap)
{
    if (scheme_len(uri) > 0) {
        return strlcpy(out, uri, cap) < cap ? ESP_OK : ESP_ERR_SUIT_OTA_SIZE;
    }
    const char *base = ctx->base_url;
    size_t blen = strlen(base);
    size_t keep;
    if (uri[0] == '/') {
        size_t sl = scheme_len(base);
        const char *host = base + sl + 1;
        while (*host == '/') {
            host++;
        }
        const char *path = strchr(host, '/');
        keep = path ? (size_t)(path - base) : blen;
    } else {
        const char *last = strrchr(base, '/');
        keep = last ? (size_t)(last - base) + 1 : blen;
    }
    if (keep + strlen(uri) + 1 > cap) {
        return ESP_ERR_SUIT_OTA_SIZE;
    }
    memcpy(out, base, keep);
    strcpy(out + keep, uri);
    return ESP_OK;
}

static esp_err_t redirect_to(struct esp_suit_ota_ctx *ctx, size_t index)
{
    suit_component_t *c = &ctx->comp[index];
    esp_suit_ota_transport_t *t = ctx->t;
    esp_err_t err = resolve_uri(ctx, c->uri, ctx->url_buf, sizeof(ctx->url_buf));
    if (err != ESP_OK) {
        return err;
    }
    esp_suit_ota_mode_t mode = mode_of(ctx->url_buf);     /* the scheme of the URI names the link */
    if (getter_for(t, mode)->get == NULL) {
        ESP_LOGE(TAG, "no getter registered for %s", ctx->url_buf);
        return ESP_ERR_SUIT_OTA_NO_TRANSPORT;
    }
    err = suit_stream_open(ctx, index);
    if (err != ESP_OK) {
        return err;
    }
    if (mode != t->mode) {
        suit_close_getters(t);             /* another link: let the open one go */
        t->mode = mode;
    }
    t->url = ctx->url_buf;
    t->offset = 0;
    t->len = c->size;
    t->fetch_args = c->fetch_args;
    t->fetch_args_len = c->fetch_args_len;
    t->component = index;
    t->epoch++;
    t->allow_not_modified = false;
    ctx->fr.state = SUIT_FR_URL;
    esp_suit_ota_event_component_t ev = { .index = (uint8_t)index, .size = c->size };
    strlcpy(ev.id, c->id, sizeof(ev.id));
    strlcpy(ev.uri, ctx->url_buf, sizeof(ev.uri));
    suit_post_event(ESP_SUIT_OTA_EVENT_COMPONENT_NEXT, &ev, sizeof(ev));
    ESP_LOGI(TAG, "fetching %s from %s (%lu bytes)", c->id, ctx->url_buf, (unsigned long)c->size);
    return ESP_OK;
}

static esp_err_t become_ready(struct esp_suit_ota_ctx *ctx)
{
    ctx->fr.state = SUIT_FR_READY;
    ctx->st = SUIT_ST_READY;
    if (ctx->t != NULL) {
        ctx->t->done = true;
        ctx->t->len = 0;
    }
    suit_post_event(ESP_SUIT_OTA_EVENT_READY, NULL, 0);
    ESP_LOGI(TAG, "every component staged and verified, call esp_suit_ota_commit()");
    return ESP_SUIT_OTA_READY;
}

esp_err_t suit_after_envelope(struct esp_suit_ota_ctx *ctx)
{
    if (!ctx->planned) {
        ESP_LOGE(TAG, "envelope ended before the install sequence was available");
        return ESP_ERR_SUIT_OTA_SEVERED;
    }
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->state != SUIT_COMP_PENDING) {
            continue;
        }
        if (c->integrated) {
            ESP_LOGE(TAG, "integrated payload for %s is missing from the envelope", c->id);
            return ESP_ERR_SUIT_OTA_INCOMPLETE;
        }
        if (ctx->t == NULL) {       /* nothing enclosed it and there is nowhere to fetch it from */
            ESP_LOGE(TAG, "%s needs a transport but the campaign is a stream", c->id);
            return ESP_ERR_SUIT_OTA_NO_TRANSPORT;
        }
    }
    while (ctx->order_pos < ctx->num_order) {
        size_t idx = ctx->order[ctx->order_pos++];
        suit_component_t *c = &ctx->comp[idx];
        if (c->state != SUIT_COMP_PENDING) {
            continue;
        }
        return redirect_to(ctx, idx);
    }
    return become_ready(ctx);
}

/* ---------- manifest completion ---------- */

esp_err_t suit_run_plan_if_ready(struct esp_suit_ota_ctx *ctx)
{
    if (ctx->planned || ctx->needed_sections != 0) {
        return ESP_OK;
    }
    esp_err_t err = suit_exec_plan(ctx);
    if (err != ESP_OK) {
        if (err == ESP_ERR_SUIT_OTA_CONDITION) {
            ESP_LOGE(TAG, "manifest is not for this device");
        }
        return err;
    }
    ctx->planned = true;
    err = suit_map_check_groups(ctx);
    if (err != ESP_OK) {
        return err;
    }
    if (ctx->campaign_id == 0) {
        ctx->campaign_id = (uint32_t)ctx->man.sequence_number ^ ((uint32_t)ctx->manifest_digest[0] << 24) ^
                           ((uint32_t)ctx->manifest_digest[1] << 16) ^ 1u;
    }
    size_t pending = 0;
    for (size_t i = 0; i < ctx->num_comp; i++) {
        if (ctx->comp[i].state == SUIT_COMP_PENDING) {
            pending++;
        }
    }
    esp_suit_ota_event_manifest_t ev = {
        .sequence_number = ctx->man.sequence_number,
        .num_components = (uint8_t)ctx->num_comp,
        .num_pending = (uint8_t)pending,
    };
    strlcpy(ev.description, ctx->description, sizeof(ev.description));
    suit_post_event(ESP_SUIT_OTA_EVENT_MANIFEST_VERIFIED, &ev, sizeof(ev));
    ESP_LOGI(TAG, "manifest verified: sequence %llu, %u components, %u to fetch",
             (unsigned long long)ctx->man.sequence_number, (unsigned)ctx->num_comp, (unsigned)pending);
    if (pending == 0 && ctx->man.sequence_number == suit_persist_installed_sequence()) {
        ESP_LOGI(TAG, "same release, every component already installed");
        return ESP_ERR_SUIT_OTA_UP_TO_DATE;
    }
    ctx->has_app = false;
    ctx->app_slot = ESP_SUIT_OTA_APP_SLOT_NONE;
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->target.is_app && c->state != SUIT_COMP_INSTALLED) {
            ctx->has_app = true;
            ctx->app_slot = c->target.slot_index;
        }
    }
    return ESP_OK;
}

esp_err_t suit_on_manifest_complete(struct esp_suit_ota_ctx *ctx)
{
    esp_err_t err = suit_cose_verify(ctx->auth.ptr, ctx->auth.len, ctx->manifest_bstr.ptr, ctx->manifest_bstr.len,
                                     ctx->cfg.keys, ctx->cfg.num_keys, ctx->manifest_digest);
    if (err != ESP_OK) {
        return err;
    }
    suit_cbor_head_t h;
    int hl = suit_cbor_head(ctx->manifest_bstr.ptr, ctx->manifest_bstr.len, &h);
    if (hl <= 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    err = suit_manifest_parse(ctx->manifest_bstr.ptr + hl, ctx->manifest_bstr.len - hl, &ctx->man);
    if (err != ESP_OK) {
        return err;
    }
    uint64_t installed = suit_persist_installed_sequence();
    if (ctx->man.sequence_number < installed) {
        ESP_LOGE(TAG, "sequence number %llu is below the installed %llu",
                 (unsigned long long)ctx->man.sequence_number, (unsigned long long)installed);
        return ESP_ERR_SUIT_OTA_SEQUENCE;
    }
    ctx->num_comp = ctx->man.num_components;
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        memset(c, 0, sizeof(*c));
        suit_cid_to_string(ctx->man.cid_parts[i], ctx->man.cid_num_parts[i], c->id, sizeof(c->id));
        err = suit_map_resolve(ctx, i);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = suit_exec_check_sequence_shape(ctx, &ctx->man.shared_seq, true);
    if (err == ESP_OK) {
        err = suit_exec_check_sequence_shape(ctx, &ctx->man.validate, false);
    }
    if (err == ESP_OK && ctx->man.install.present && !ctx->man.install.severed) {
        err = suit_exec_check_sequence_shape(ctx, &ctx->man.install.bytes, false);
    }
    if (err == ESP_OK && ctx->man.payload_fetch.present && !ctx->man.payload_fetch.severed) {
        err = suit_exec_check_sequence_shape(ctx, &ctx->man.payload_fetch.bytes, false);
    }
    if (err != ESP_OK) {
        return err;
    }
    if (ctx->man.text_inline.ptr != NULL) {
        const char *lang = ctx->cfg.text_lang ? ctx->cfg.text_lang : CONFIG_ESP_SUIT_OTA_TEXT_LANG;
        suit_text_description(ctx->man.text_inline.ptr, ctx->man.text_inline.len, lang, ctx->description,
                              sizeof(ctx->description));
    }
    ctx->needed_sections = 0;
    if (ctx->man.install.present && ctx->man.install.severed) {
        ctx->needed_sections |= 1u << SUIT_ENV_INSTALL;
    }
    if (ctx->man.payload_fetch.present && ctx->man.payload_fetch.severed) {
        ctx->needed_sections |= 1u << SUIT_ENV_PAYLOAD_FETCH;
    }
    ctx->manifest_verified = true;
    return suit_run_plan_if_ready(ctx);
}

esp_err_t suit_on_severed_section(struct esp_suit_ota_ctx *ctx, int key, const uint8_t *bstr_with_head,
                                  size_t len, const uint8_t digest[SUIT_DIGEST_LEN])
{
    suit_section_t *s = (key == SUIT_ENV_INSTALL) ? &ctx->man.install : &ctx->man.payload_fetch;
    if (memcmp(digest, s->digest, SUIT_DIGEST_LEN) != 0) {
        ESP_LOGE(TAG, "severed section %d has the wrong digest", key);
        return ESP_ERR_SUIT_OTA_SEVERED;
    }
    suit_cbor_head_t h;
    int hl = suit_cbor_head(bstr_with_head, len, &h);
    if (hl <= 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    s->bytes.ptr = bstr_with_head + hl;
    s->bytes.len = len - hl;
    s->have_bytes = true;
    esp_err_t err = suit_exec_check_sequence_shape(ctx, &s->bytes, false);
    if (err != ESP_OK) {
        return err;
    }
    ctx->needed_sections &= ~(1u << key);
    return suit_run_plan_if_ready(ctx);
}

esp_err_t suit_on_text_bytes(struct esp_suit_ota_ctx *ctx, const uint8_t *content, size_t len,
                             const uint8_t digest[SUIT_DIGEST_LEN])
{
    if (memcmp(digest, ctx->man.text.digest, SUIT_DIGEST_LEN) != 0) {
        ESP_LOGE(TAG, "severed text has the wrong digest");
        return ESP_ERR_SUIT_OTA_SEVERED;
    }
    if (content != NULL) {
        const char *lang = ctx->cfg.text_lang ? ctx->cfg.text_lang : CONFIG_ESP_SUIT_OTA_TEXT_LANG;
        suit_text_description(content, len, lang, ctx->description, sizeof(ctx->description));
    }
    return ESP_OK;
}

/*
 * Draft section 7.5: the envelope key of an enclosed payload is the component's URI. A payload the
 * manifest author enclosed carries the fragment-only reference as written, "#f1". A payload an
 * intermediary pre-fetched carries the full URI it came from. Either way the key must equal the
 * URI byte for byte: "f1" is a different resource from "#f1".
 */
int suit_find_integrated_component(struct esp_suit_ota_ctx *ctx, const char *name, size_t name_len)
{
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        if (c->state != SUIT_COMP_PENDING || !c->has_uri) {
            continue;
        }
        size_t ulen = strlen(c->uri);
        if (ulen == name_len && memcmp(c->uri, name, name_len) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ---------- payload bytes ---------- */

esp_err_t suit_payload_write(struct esp_suit_ota_ctx *ctx, const uint8_t *data, size_t len)
{
    size_t idx = ctx->current;
    esp_err_t err = suit_stream_write(ctx, idx, data, len);
    if (err != ESP_OK) {
        return err;
    }
    suit_component_t *c = &ctx->comp[idx];
    if (c->bytes_written - ctx->progress_mark >= SUIT_PROGRESS_STEP_BYTES) {
        ctx->progress_mark = c->bytes_written;
        esp_suit_ota_event_progress_t ev = { .index = (uint8_t)idx, .bytes_written = c->bytes_written, .size = c->size };
        suit_post_event(ESP_SUIT_OTA_EVENT_PROGRESS, &ev, sizeof(ev));
    }
    return ESP_OK;
}

esp_err_t suit_payload_boundary(struct esp_suit_ota_ctx *ctx)
{
    size_t idx = ctx->current;
    esp_err_t err = suit_stream_close(ctx, idx);
    if (err != ESP_OK) {
        return err;
    }
    ctx->progress_mark = 0;
    return ESP_OK;
}

/* ---------- API ---------- */

void suit_ctx_free(struct esp_suit_ota_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->lock != NULL) {
        vSemaphoreDelete(ctx->lock);
    }
    free(ctx->arena);
    free(ctx);
}

esp_err_t esp_suit_ota_begin(const esp_suit_ota_config_t *cfg, esp_suit_ota_transport_t *t,
                             esp_suit_ota_handle_t *out)
{
    bool have_vendor = (cfg != NULL) && ((cfg->vendor_ids != NULL && cfg->num_vendor_ids > 0) ||
                                         (cfg->vendor_pens != NULL && cfg->num_vendor_pens > 0));
    if (cfg == NULL || out == NULL || cfg->keys == NULL || cfg->num_keys == 0 ||
            !have_vendor || cfg->class_ids == NULL || cfg->num_class_ids == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t != NULL && t->url == NULL) {
        return ESP_ERR_INVALID_ARG;         /* where the envelope comes from */
    }
    *out = NULL;
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGE(TAG, "the running app is not confirmed yet");
        return ESP_ERR_SUIT_OTA_STATE;
    }
    const esp_suit_ota_slots_ops_t *ops = cfg->slots_ops;
    if (ops != NULL && ops->is_pending != NULL && ops->is_pending(ops->user)) {
        ESP_LOGE(TAG, "a release is activated but not settled yet; confirm it or revert it first");
        return ESP_ERR_SUIT_OTA_STATE;
    }
    struct esp_suit_ota_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->arena_size = CONFIG_ESP_SUIT_OTA_ENVELOPE_BUF;
    ctx->arena = malloc(ctx->arena_size);
    ctx->lock = xSemaphoreCreateRecursiveMutex();
    if (ctx->arena == NULL || ctx->lock == NULL) {
        suit_ctx_free(ctx);
        return ESP_ERR_NO_MEM;
    }
    ctx->cfg = *cfg;
    ctx->t = t;
    ctx->current = SIZE_MAX;
    ctx->st = SUIT_ST_FEEDING;
    if (t != NULL) {
        strlcpy(ctx->base_url, t->url, sizeof(ctx->base_url));
        t->mode = mode_of(t->url);
        if (getter_for(t, t->mode)->get == NULL) {
            ESP_LOGE(TAG, "no getter registered for %s", t->url);
            suit_ctx_free(ctx);
            return ESP_ERR_SUIT_OTA_NO_TRANSPORT;
        }
        t->offset = 0;
        t->len = ESP_SUIT_OTA_LEN_UNKNOWN;
        t->fetch_args = NULL;
        t->fetch_args_len = 0;
        t->component = SIZE_MAX;
        t->epoch++;
        t->done = false;
        t->allow_not_modified = true;
        t->manifest_etag[0] = '\0';
    }
    *out = ctx;
    return ESP_OK;
}

static void fail(struct esp_suit_ota_ctx *ctx, esp_err_t err)
{
    if (ctx->current != SIZE_MAX) {
        suit_stream_abort(ctx, ctx->current);
    }
    ctx->sticky_err = err;
    suit_post_abort(ctx, err, (uint8_t)ctx->fr.state);     /* the phase where it failed, then FAILED */
    ctx->fr.state = SUIT_FR_FAILED;
    ESP_LOGE(TAG, "campaign failed: %s (0x%x)", esp_suit_ota_err_to_name(err) ? esp_suit_ota_err_to_name(err)
             : esp_err_to_name(err), err);
}

esp_err_t esp_suit_ota_feed(esp_suit_ota_handle_t h, const void *data, size_t len, size_t *used)
{
    if (h == NULL || used == NULL || (data == NULL && len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    *used = 0;
    LOCK(h);
    esp_err_t err;
    if (h->sticky_err != ESP_OK) {
        err = h->sticky_err;
    } else if (h->st == SUIT_ST_READY && len > 0) {
        /* the same bytes in the buffer that ended the campaign fail in the framer: so must these */
        ESP_LOGE(TAG, "bytes after the campaign became ready");
        err = ESP_ERR_SUIT_OTA_FORMAT;
        fail(h, err);
    } else if (h->st != SUIT_ST_FEEDING) {
        err = (h->st == SUIT_ST_READY) ? ESP_SUIT_OTA_READY : ESP_ERR_SUIT_OTA_STATE;
    } else {
        err = suit_frame_feed(h, data, len, used);
        if (err != ESP_OK && err != ESP_SUIT_OTA_READY) {
            fail(h, err);
        }
    }
    UNLOCK(h);
    return err;
}

esp_err_t esp_suit_ota_commit(esp_suit_ota_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    LOCK(h);
    esp_err_t err;
    if (h->sticky_err != ESP_OK) {
        err = h->sticky_err;
    } else if (h->st != SUIT_ST_READY) {
        err = ESP_ERR_SUIT_OTA_STATE;
    } else {
        err = suit_exec_validate(h);
        if (err == ESP_OK) {
            h->st = SUIT_ST_COMMITTED;
            ESP_LOGI(TAG, "validate sequence passed");
        } else {
            if (err == ESP_ERR_SUIT_OTA_CONDITION) {
                err = ESP_ERR_SUIT_OTA_DIGEST;
            }
            fail(h, err);
        }
    }
    UNLOCK(h);
    return err;
}

esp_err_t esp_suit_ota_end(esp_suit_ota_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    LOCK(h);
    if (h->current != SIZE_MAX) {
        suit_stream_abort(h, h->current);
    }
    suit_close_getters(h->t);
    UNLOCK(h);
    suit_ctx_free(h);
    return ESP_OK;
}

esp_err_t esp_suit_ota_get_info(esp_suit_ota_handle_t h, esp_suit_ota_info_t *info)
{
    if (h == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    LOCK(h);
    if (!h->manifest_verified) {
        UNLOCK(h);
        return ESP_ERR_SUIT_OTA_STATE;
    }
    memset(info, 0, sizeof(*info));
    info->sequence_number = h->man.sequence_number;
    info->num_components = h->num_comp;
    for (size_t i = 0; i < h->num_comp; i++) {
        if (h->comp[i].state == SUIT_COMP_PENDING || h->comp[i].state == SUIT_COMP_WRITING) {
            info->num_pending++;
        }
    }
    strlcpy(info->description, h->description, sizeof(info->description));
    UNLOCK(h);
    return ESP_OK;
}

esp_err_t esp_suit_ota_get_component(esp_suit_ota_handle_t h, size_t index, esp_suit_ota_component_t *out)
{
    if (h == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    LOCK(h);
    if (!h->manifest_verified || index >= h->num_comp) {
        UNLOCK(h);
        return ESP_ERR_INVALID_ARG;
    }
    suit_component_t *c = &h->comp[index];
    memset(out, 0, sizeof(*out));
    strlcpy(out->id, c->id, sizeof(out->id));
    strlcpy(out->uri, c->uri, sizeof(out->uri));
    out->integrated = c->integrated;
    out->size = c->size;
    memcpy(out->digest, c->digest, SUIT_DIGEST_LEN);
    out->verify = c->target.verify;
    out->bytes_written = c->bytes_written;
    switch (c->state) {
    case SUIT_COMP_INSTALLED: out->state = ESP_SUIT_COMP_ALREADY_INSTALLED; break;
    case SUIT_COMP_STAGED: out->state = ESP_SUIT_COMP_STAGED_VERIFIED; break;
    default: out->state = ESP_SUIT_COMP_NEEDED; break;
    }
    UNLOCK(h);
    return ESP_OK;
}

esp_err_t esp_suit_ota_get_app_desc(esp_suit_ota_handle_t h, size_t index, esp_app_desc_t *desc)
{
    if (h == NULL || desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    LOCK(h);
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (index < h->num_comp) {
        if (h->comp[index].has_app_desc) {
            memcpy(desc, &h->comp[index].app_desc, sizeof(*desc));
            err = ESP_OK;
        } else {
            err = ESP_ERR_SUIT_OTA_STATE;
        }
    }
    UNLOCK(h);
    return err;
}

esp_err_t esp_suit_ota_get_installed_info(esp_suit_ota_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    suit_installed_t r;
    memset(info, 0, sizeof(*info));
    if (suit_persist_installed_read(&r) != ESP_OK) {
        info->sequence_number = CONFIG_ESP_SUIT_OTA_INITIAL_SEQ;
        return ESP_ERR_NOT_FOUND;
    }
    info->sequence_number = r.sequence_number;
    info->num_components = r.num;
    strlcpy(info->description, r.description, sizeof(info->description));
    return ESP_OK;
}
