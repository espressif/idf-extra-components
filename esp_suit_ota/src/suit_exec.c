/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"

static const char *TAG = SUIT_TAG;

/* A condition failed while soft-failure was set: the sequence stops, the caller carries on. */
#define SUIT_RES_SOFT_STOP  ((esp_err_t)0x1F0FF)

typedef struct {
    bool has_vendor, has_class, has_device, has_digest, has_size, has_slot, has_uri;
    bool vendor_is_pen;        /* the vendor id arrived as a relative OID, not as a UUID */
    uint32_t vendor_pen;
    uint8_t vendor[SUIT_UUID_LEN];
    uint8_t class_id[SUIT_UUID_LEN];
    uint8_t device[SUIT_UUID_LEN];
    uint8_t digest[SUIT_DIGEST_LEN];
    uint64_t size;
    uint64_t slot;
    suit_span_t uri;
    suit_span_t fetch_args;
} suit_params_t;

typedef struct {
    struct esp_suit_ota_ctx *ctx;
    suit_exec_mode_t mode;
    suit_params_t params[SUIT_MAX_COMP];
    uint8_t idx[SUIT_MAX_COMP];
    size_t idx_count;
    bool idx_set;
    bool in_shared;
    bool seen_vendor_check;
    bool seen_class_check;
    bool cond_hard;            /* a try-each was exhausted: not softened by any enclosing sequence */
    bool branch_not_last;      /* inside a try-each branch that has a successor */
} suit_exec_t;

static esp_err_t run_sequence(suit_exec_t *ex, const uint8_t *seq, size_t len, int depth, bool soft_default);

static bool any_uuid_match(const uint8_t (*set)[SUIT_UUID_LEN], size_t n, const uint8_t *id)
{
    for (size_t i = 0; i < n; i++) {
        if (memcmp(set[i], id, SUIT_UUID_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t target_matches_digest(struct esp_suit_ota_ctx *ctx, size_t i, bool staging, const uint8_t *digest,
                                       bool *match)
{
    uint8_t actual[SUIT_DIGEST_LEN];
    esp_err_t err = suit_stream_digest_of_target(ctx, i, staging, actual);
    if (err == ESP_ERR_NOT_SUPPORTED) {
        *match = false;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    *match = (memcmp(actual, digest, SUIT_DIGEST_LEN) == 0);
    return ESP_OK;
}

static esp_err_t cond_image_match(suit_exec_t *ex, size_t i)
{
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    suit_params_t *p = &ex->params[i];
    suit_component_t *c = &ctx->comp[i];
    if (!p->has_digest) {
        return ESP_ERR_SUIT_OTA_CONDITION;
    }
    bool match = false;
    esp_err_t err;
    if (ex->mode == SUIT_EXEC_PLAN) {
        switch (c->state) {
        case SUIT_COMP_INSTALLED:
            return memcmp(c->digest, p->digest, SUIT_DIGEST_LEN) == 0 ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
        case SUIT_COMP_PENDING:
        case SUIT_COMP_WRITING:
            if (ex->branch_not_last) {
                ESP_LOGE(TAG, "image-match after fetch is only supported in the last try-each branch");
                return ESP_ERR_SUIT_OTA_UNSUPPORTED;
            }
            return ESP_OK;                         /* checked at the payload boundary */
        case SUIT_COMP_STAGED:
            return memcmp(c->digest, p->digest, SUIT_DIGEST_LEN) == 0 ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
        default:
            break;
        }
        if (!p->has_size) {
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
        c->size = (uint32_t)p->size;
        err = target_matches_digest(ctx, i, false, p->digest, &match);
        if (err != ESP_OK) {
            return err;
        }
        if (!match) {
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
        c->state = SUIT_COMP_INSTALLED;
        memcpy(c->digest, p->digest, SUIT_DIGEST_LEN);
        c->has_digest = true;
        c->has_size = true;
        return ESP_OK;
    }
    /* VALIDATE: read the target that will be live after activation */
    if (c->state == SUIT_COMP_STAGED) {
        err = target_matches_digest(ctx, i, true, p->digest, &match);
    } else if (c->state == SUIT_COMP_INSTALLED) {
        err = target_matches_digest(ctx, i, false, p->digest, &match);
    } else {
        return ESP_ERR_SUIT_OTA_CONDITION;
    }
    if (err != ESP_OK) {
        return err;
    }
    return match ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
}

static esp_err_t run_condition(suit_exec_t *ex, int64_t cmd, size_t i)
{
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    suit_params_t *p = &ex->params[i];
    switch (cmd) {
    case SUIT_CMD_COND_VENDOR:
        if (ex->in_shared) {
            ex->seen_vendor_check = true;
        }
        if (!p->has_vendor) {
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
        if (p->vendor_is_pen) {
            for (size_t k = 0; k < ctx->cfg.num_vendor_pens; k++) {
                if (ctx->cfg.vendor_pens[k] == p->vendor_pen) {
                    return ESP_OK;
                }
            }
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
        return any_uuid_match(ctx->cfg.vendor_ids, ctx->cfg.num_vendor_ids, p->vendor)
               ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
    case SUIT_CMD_COND_CLASS:
        if (ex->in_shared) {
            ex->seen_class_check = true;
        }
        return (p->has_class && any_uuid_match(ctx->cfg.class_ids, ctx->cfg.num_class_ids, p->class_id))
               ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
    case SUIT_CMD_COND_DEVICE:
        return (p->has_device && ctx->cfg.device_id != NULL && memcmp(ctx->cfg.device_id, p->device, SUIT_UUID_LEN) == 0)
               ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
    case SUIT_CMD_COND_IMAGE_MATCH:
        return cond_image_match(ex, i);
    case SUIT_CMD_COND_SLOT:
        return (p->has_slot && p->slot == ctx->comp[i].target.slot_index) ? ESP_OK : ESP_ERR_SUIT_OTA_CONDITION;
    case SUIT_CMD_COND_ABORT:
        return ESP_ERR_SUIT_OTA_CONDITION;
    default:
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
}

static esp_err_t parse_uuid_param(suit_cbor_t *c, uint8_t out[SUIT_UUID_LEN], bool *flag)
{
    const uint8_t *s;
    size_t slen;
    if (suit_cbor_get_bstr(c, &s, &slen) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (slen != SUIT_UUID_LEN) {
        ESP_LOGE(TAG, "identifier is %u bytes, a UUID is %u", (unsigned)slen, (unsigned)SUIT_UUID_LEN);
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    memcpy(out, s, SUIT_UUID_LEN);
    *flag = true;
    return ESP_OK;
}

/*
 * Draft section 8.4.8.3: a vendor identifier is a UUID in a bstr, or a private enterprise number
 * encoded as cbor-pen = #6.112(bstr), the RFC 9090 relative OID under 1.3.6.1.4.1.
 */
static esp_err_t parse_vendor_param(suit_cbor_t *c, suit_params_t *p)
{
    suit_cbor_head_t h;
    if (suit_cbor_peek(c, &h) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (h.major != SUIT_CBOR_TAG) {
        p->vendor_is_pen = false;
        return parse_uuid_param(c, p->vendor, &p->has_vendor);
    }
    uint64_t tag;
    if (suit_cbor_get_tag(c, &tag) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (tag != SUIT_CBOR_TAG_PEN) {
        ESP_LOGE(TAG, "vendor identifier tag %llu is not cbor-pen (%u)", (unsigned long long)tag, (unsigned)SUIT_CBOR_TAG_PEN);
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    const uint8_t *s;
    size_t slen;
    if (suit_cbor_get_bstr(c, &s, &slen) != ESP_OK || slen == 0 || s[0] == SUIT_OID_ARC_MORE) {
        return ESP_ERR_SUIT_OTA_FORMAT;             /* RFC 9090: an arc never starts with an empty 7-bit group */
    }
    uint64_t arc = 0;
    size_t i = 0;
    for (; i < slen; i++) {
        if (arc > (UINT64_MAX >> SUIT_OID_ARC_BITS)) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        arc = (arc << SUIT_OID_ARC_BITS) | (uint64_t)(s[i] & SUIT_OID_ARC_VALUE_MASK);
        if ((s[i] & SUIT_OID_ARC_MORE) == 0) {
            i++;
            break;
        }
    }
    if (i != slen || arc > UINT32_MAX) {        /* one arc: the enterprise number itself */
        ESP_LOGE(TAG, "vendor identifier is not a single enterprise number arc");
        return ESP_ERR_SUIT_OTA_CONDITION;
    }
    p->vendor_pen = (uint32_t)arc;
    p->vendor_is_pen = true;
    p->has_vendor = true;
    return ESP_OK;
}

static esp_err_t apply_override(suit_exec_t *ex, suit_cbor_t *c, int depth, bool *soft_failure)
{
    size_t n;
    if (suit_cbor_get_map(c, &n) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    int64_t last = -1;
    for (size_t k = 0; k < n; k++) {
        int64_t key;
        if (suit_cbor_get_int(c, &key) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (key < 0) {
            ESP_LOGE(TAG, "custom parameter %lld is not supported", (long long)key);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (key <= last) {                          /* canonical order, so no key appears twice */
            ESP_LOGE(TAG, "parameter keys are not in canonical order");
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        last = key;
        /* processor-scoped parameters */
        if (key == SUIT_PARAM_STRICT_ORDER) {
            bool v;
            if (suit_cbor_get_bool(c, &v) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            continue;                               /* serial execution is always valid */
        }
        if (key == SUIT_PARAM_SOFT_FAILURE) {
            bool v;
            if (suit_cbor_get_bool(c, &v) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            *soft_failure = v;      /* processor scoped, and legal at any depth */
            continue;
        }
        const uint8_t *value;
        size_t value_len;
        if (suit_cbor_get_raw(c, &value, &value_len) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        for (size_t j = 0; j < ex->idx_count; j++) {
            suit_params_t *p = &ex->params[ex->idx[j]];
            suit_cbor_t v;
            suit_cbor_init(&v, value, value_len);
            esp_err_t err = ESP_OK;
            switch (key) {
            case SUIT_PARAM_VENDOR:
                err = parse_vendor_param(&v, p);
                break;
            case SUIT_PARAM_CLASS:
                err = parse_uuid_param(&v, p->class_id, &p->has_class);
                break;
            case SUIT_PARAM_DEVICE:
                err = parse_uuid_param(&v, p->device, &p->has_device);
                break;
            case SUIT_PARAM_DIGEST: {
                const uint8_t *d;
                size_t dlen;
                if (suit_cbor_get_bstr(&v, &d, &dlen) != ESP_OK) {
                    return ESP_ERR_SUIT_OTA_FORMAT;
                }
                suit_cbor_t dc;
                suit_cbor_init(&dc, d, dlen);
                size_t cnt;
                int64_t alg;
                const uint8_t *db;
                size_t dblen;
                if (suit_cbor_get_array(&dc, &cnt) != ESP_OK || cnt != SUIT_DIGEST_ITEMS || suit_cbor_get_int(&dc, &alg) != ESP_OK ||
                        suit_cbor_get_bstr(&dc, &db, &dblen) != ESP_OK) {
                    return ESP_ERR_SUIT_OTA_FORMAT;
                }
                if (alg != SUIT_DIGEST_ALG_SHA256) {
                    return ESP_ERR_SUIT_OTA_UNSUPPORTED;
                }
                if (dblen != SUIT_DIGEST_LEN) {
                    return ESP_ERR_SUIT_OTA_FORMAT;
                }
                memcpy(p->digest, db, SUIT_DIGEST_LEN);
                p->has_digest = true;
                break;
            }
            case SUIT_PARAM_SIZE:
                err = suit_cbor_get_uint(&v, &p->size);
                p->has_size = (err == ESP_OK);
                break;
            case SUIT_PARAM_SLOT:
                err = suit_cbor_get_uint(&v, &p->slot);
                p->has_slot = (err == ESP_OK);
                break;
            case SUIT_PARAM_URI:
                err = suit_cbor_get_tstr(&v, &p->uri.ptr, &p->uri.len);
                p->has_uri = (err == ESP_OK);
                break;
            case SUIT_PARAM_FETCH_ARGS:
                err = suit_cbor_get_bstr(&v, &p->fetch_args.ptr, &p->fetch_args.len);
                break;
            case SUIT_PARAM_CONTENT:
            case SUIT_PARAM_SOURCE_COMP:
            case SUIT_PARAM_INVOKE_ARGS:
                err = suit_cbor_skip(&v);              /* accepted; the directives that use them are not */
                break;
            default:
                ESP_LOGE(TAG, "parameter %lld is not supported", (long long)key);
                return ESP_ERR_SUIT_OTA_UNSUPPORTED;
            }
            if (err != ESP_OK) {
                return err == ESP_ERR_SUIT_OTA_UNSUPPORTED ? err : ESP_ERR_SUIT_OTA_FORMAT;
            }
        }
    }
    return ESP_OK;
}

static esp_err_t do_fetch(suit_exec_t *ex, size_t i)
{
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    suit_params_t *p = &ex->params[i];
    suit_component_t *c = &ctx->comp[i];
    if (ex->mode != SUIT_EXEC_PLAN) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    if (c->state == SUIT_COMP_INSTALLED) {
        return ESP_OK;
    }
    if (c->state != SUIT_COMP_NONE) {
        ESP_LOGE(TAG, "component %s is fetched twice", c->id);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    if (!p->has_uri || !p->has_size || !p->has_digest) {
        ESP_LOGE(TAG, "fetch of %s needs uri, image-size and image-digest", c->id);
        return p->has_size ? ESP_ERR_SUIT_OTA_CONDITION : ESP_ERR_SUIT_OTA_SIZE;
    }
    if (p->uri.len == 0 || p->uri.len >= SUIT_URI_MAX) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    if (p->size > UINT32_MAX) {
        return ESP_ERR_SUIT_OTA_SIZE;
    }
    c->size = (uint32_t)p->size;
    c->has_size = true;
    memcpy(c->digest, p->digest, SUIT_DIGEST_LEN);
    c->has_digest = true;
    memcpy(c->uri, p->uri.ptr, p->uri.len);
    c->uri[p->uri.len] = '\0';
    c->has_uri = true;
    c->integrated = (c->uri[0] == '#');
    c->fetch_args = p->fetch_args.ptr;
    c->fetch_args_len = p->fetch_args.len;

    suit_target_t *tg = &c->target;
    if (tg->staging != NULL && c->size > tg->staging->size) {
        ESP_LOGE(TAG, "%s: %lu bytes do not fit %s (%lu)", c->id, (unsigned long)c->size,
                 tg->staging->label, (unsigned long)tg->staging->size);
        return ESP_ERR_SUIT_OTA_SIZE;
    }
#if CONFIG_ESP_SUIT_OTA_SKIP_IF_INSTALLED
    if (tg->active != NULL) {
        bool match = false;
        esp_err_t err = target_matches_digest(ctx, i, false, c->digest, &match);
        if (err != ESP_OK) {
            return err;
        }
        if (match) {
            c->state = SUIT_COMP_INSTALLED;
            esp_suit_ota_event_component_t ev = { .index = (uint8_t)i, .size = c->size };
            strlcpy(ev.id, c->id, sizeof(ev.id));
            strlcpy(ev.uri, c->uri, sizeof(ev.uri));
            suit_post_event(ESP_SUIT_OTA_EVENT_COMPONENT_SKIPPED, &ev, sizeof(ev));
            ESP_LOGI(TAG, "%s already installed, skipped", c->id);
            return ESP_OK;
        }
    }
#endif
    c->state = SUIT_COMP_PENDING;
    ctx->order[ctx->num_order++] = (uint8_t)i;
    return ESP_OK;
}

static esp_err_t set_index(suit_exec_t *ex, suit_cbor_t *c)
{
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    suit_cbor_head_t h;
    if (suit_cbor_peek(c, &h) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    ex->idx_count = 0;
    if (h.major == SUIT_CBOR_UINT) {
        uint64_t v;
        suit_cbor_get_uint(c, &v);
        if (v >= ctx->num_comp) {
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        ex->idx[ex->idx_count++] = (uint8_t)v;
    } else if (h.major == SUIT_CBOR_SIMPLE) {
        bool b;
        if (suit_cbor_get_bool(c, &b) != ESP_OK || !b) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        for (size_t i = 0; i < ctx->num_comp; i++) {
            ex->idx[ex->idx_count++] = (uint8_t)i;
        }
    } else if (h.major == SUIT_CBOR_ARRAY) {
        size_t n;
        suit_cbor_get_array(c, &n);
        if (n == 0 || n > SUIT_MAX_COMP) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        for (size_t k = 0; k < n; k++) {
            uint64_t v;
            if (suit_cbor_get_uint(c, &v) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            if (v >= ctx->num_comp) {
                return ESP_ERR_SUIT_OTA_UNSUPPORTED;
            }
            ex->idx[ex->idx_count++] = (uint8_t)v;
        }
    } else {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    ex->idx_set = true;
    return ESP_OK;
}

/* run a nested sequence once per selected component with a single-element index set */
static esp_err_t run_per_component(suit_exec_t *ex, const uint8_t *seq, size_t len, int depth, bool soft_default,
                                   size_t i)
{
    uint8_t saved_idx[SUIT_MAX_COMP];
    size_t saved_count = ex->idx_count;
    memcpy(saved_idx, ex->idx, sizeof(saved_idx));
    ex->idx[0] = (uint8_t)i;
    ex->idx_count = 1;
    esp_err_t r = run_sequence(ex, seq, len, depth, soft_default);
    memcpy(ex->idx, saved_idx, sizeof(saved_idx));
    ex->idx_count = saved_count;
    return r;
}

static esp_err_t do_try_each(suit_exec_t *ex, suit_cbor_t *c, int depth)
{
    size_t n;
    if (suit_cbor_get_array(c, &n) != ESP_OK || n < 1) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    const uint8_t *branch[SUIT_TRY_EACH_MAX];
    size_t branch_len[SUIT_TRY_EACH_MAX];
    bool branch_nil[SUIT_TRY_EACH_MAX];
    if (n > SUIT_TRY_EACH_MAX) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    for (size_t b = 0; b < n; b++) {
        suit_cbor_head_t h;
        if (suit_cbor_peek(c, &h) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (h.major == SUIT_CBOR_SIMPLE) {
            if (suit_cbor_get_nil(c) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            branch_nil[b] = true;
            branch[b] = NULL;
            branch_len[b] = 0;
        } else {
            if (suit_cbor_get_bstr(c, &branch[b], &branch_len[b]) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            branch_nil[b] = false;
        }
    }
    uint8_t selected[SUIT_MAX_COMP];
    size_t count = ex->idx_count;
    memcpy(selected, ex->idx, sizeof(selected));
    for (size_t k = 0; k < count; k++) {
        bool done = false;
        for (size_t b = 0; b < n && !done; b++) {
            if (branch_nil[b]) {
                done = true;
                break;
            }
            bool saved_not_last = ex->branch_not_last;
            ex->branch_not_last = (b + 1 < n);
            esp_err_t r = run_per_component(ex, branch[b], branch_len[b], depth + 1, true, selected[k]);
            ex->branch_not_last = saved_not_last;
            if (r == ESP_OK) {
                done = true;
            } else if (r == SUIT_RES_SOFT_STOP) {
                continue;
            } else {
                return r;
            }
        }
        if (!done) {
            ESP_LOGE(TAG, "try-each exhausted for component %u", (unsigned)selected[k]);
            ex->cond_hard = true;
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
    }
    return ESP_OK;
}

static esp_err_t run_sequence(suit_exec_t *ex, const uint8_t *seq, size_t len, int depth, bool soft_default)
{
    if (depth > CONFIG_ESP_SUIT_OTA_MAX_NESTING) {
        ESP_LOGE(TAG, "command sequences nested deeper than %d", CONFIG_ESP_SUIT_OTA_MAX_NESTING);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    suit_cbor_t c;
    suit_cbor_init(&c, seq, len);
    size_t n;
    if (suit_cbor_get_array(&c, &n) != ESP_OK || (n % SUIT_SEQ_ITEMS_PER_CMD) != 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    bool soft_failure = soft_default;
    for (size_t k = 0; k < n; k += SUIT_SEQ_ITEMS_PER_CMD) {
        int64_t cmd;
        if (suit_cbor_get_int(&c, &cmd) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (cmd < 0) {
            ESP_LOGE(TAG, "custom command %lld is not supported", (long long)cmd);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (k == 0 && ctx->num_comp > 1 && depth == 0 && cmd != SUIT_CMD_SET_INDEX) {
            ESP_LOGE(TAG, "a sequence must start with set-component-index when there are several components");
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        esp_err_t r = ESP_OK;
        switch (cmd) {
        case SUIT_CMD_SET_INDEX:
            r = set_index(ex, &c);
            break;
        case SUIT_CMD_OVERRIDE:
            if (!ex->idx_set) {
                ex->idx[0] = 0;
                ex->idx_count = 1;
                ex->idx_set = true;
            }
            r = apply_override(ex, &c, depth, &soft_failure);
            break;
        case SUIT_CMD_COND_VENDOR:
        case SUIT_CMD_COND_CLASS:
        case SUIT_CMD_COND_DEVICE:
        case SUIT_CMD_COND_IMAGE_MATCH:
        case SUIT_CMD_COND_SLOT:
        case SUIT_CMD_COND_ABORT: {
            uint64_t policy;
            if (suit_cbor_get_uint(&c, &policy) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            if (!ex->idx_set) {
                ex->idx[0] = 0;
                ex->idx_count = 1;
                ex->idx_set = true;
            }
            for (size_t j = 0; j < ex->idx_count && r == ESP_OK; j++) {
                r = run_condition(ex, cmd, ex->idx[j]);
            }
            break;
        }
        case SUIT_CMD_FETCH: {
            uint64_t policy;
            if (suit_cbor_get_uint(&c, &policy) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            if (ex->in_shared) {
                return ESP_ERR_SUIT_OTA_UNSUPPORTED;
            }
            if (!ex->idx_set) {
                ex->idx[0] = 0;
                ex->idx_count = 1;
                ex->idx_set = true;
            }
            for (size_t j = 0; j < ex->idx_count && r == ESP_OK; j++) {
                r = do_fetch(ex, ex->idx[j]);
            }
            break;
        }
        case SUIT_CMD_TRY_EACH:
            if (!ex->idx_set) {
                ex->idx[0] = 0;
                ex->idx_count = 1;
                ex->idx_set = true;
            }
            r = do_try_each(ex, &c, depth);
            break;
        case SUIT_CMD_RUN_SEQUENCE: {
            const uint8_t *sub;
            size_t sub_len;
            if (suit_cbor_get_bstr(&c, &sub, &sub_len) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            if (!ex->idx_set) {
                ex->idx[0] = 0;
                ex->idx_count = 1;
                ex->idx_set = true;
            }
            uint8_t selected[SUIT_MAX_COMP];
            size_t count = ex->idx_count;
            memcpy(selected, ex->idx, sizeof(selected));
            for (size_t j = 0; j < count && r == ESP_OK; j++) {
                r = run_per_component(ex, sub, sub_len, depth + 1, false, selected[j]);
                if (r == SUIT_RES_SOFT_STOP) {
                    r = ESP_OK;
                }
            }
            break;
        }
        case SUIT_CMD_WRITE:
        case SUIT_CMD_COPY:
        case SUIT_CMD_SWAP:
        case SUIT_CMD_INVOKE:
        case SUIT_CMD_COND_CHECK_CONTENT:
            ESP_LOGE(TAG, "command %lld is not supported", (long long)cmd);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        default:
            ESP_LOGE(TAG, "unknown command %lld", (long long)cmd);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (r == ESP_ERR_SUIT_OTA_CONDITION) {
            if (soft_failure && !ex->cond_hard) {
                return SUIT_RES_SOFT_STOP;
            }
            return ESP_ERR_SUIT_OTA_CONDITION;
        }
        if (r != ESP_OK) {
            return r;
        }
    }
    return ESP_OK;
}

static esp_err_t run_top(suit_exec_t *ex, const suit_span_t *seq, bool shared)
{
    if (seq->ptr == NULL) {
        return ESP_OK;
    }
    ex->in_shared = shared;
    ex->idx_set = false;
    ex->idx_count = 0;
    esp_err_t r = run_sequence(ex, seq->ptr, seq->len, 0, false);
    ex->in_shared = false;
    if (r == SUIT_RES_SOFT_STOP) {
        return ESP_OK;              /* soft failure: the sequence ended early, the update goes on */
    }
    return r;
}

static void exec_init(suit_exec_t *ex, struct esp_suit_ota_ctx *ctx, suit_exec_mode_t mode)
{
    memset(ex, 0, sizeof(*ex));
    ex->ctx = ctx;
    ex->mode = mode;
}

static esp_err_t run_shared(suit_exec_t *ex)
{
    struct esp_suit_ota_ctx *ctx = ex->ctx;
    esp_err_t r = run_top(ex, &ctx->man.shared_seq, true);
    if (r != ESP_OK) {
        return r;
    }
#if CONFIG_ESP_SUIT_OTA_REQUIRE_ID_CHECKS
    if (!ex->seen_vendor_check || !ex->seen_class_check) {
        ESP_LOGE(TAG, "shared sequence lacks the vendor and class identifier checks");
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
#endif
    return ESP_OK;
}

esp_err_t suit_exec_plan(struct esp_suit_ota_ctx *ctx)
{
    suit_exec_t *ex = calloc(1, sizeof(*ex));
    if (ex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    exec_init(ex, ctx, SUIT_EXEC_PLAN);
    ctx->num_order = 0;
    esp_err_t r = run_shared(ex);
    if (r == ESP_OK) {
        if (!ctx->man.payload_fetch.present && !ctx->man.install.present) {
            ESP_LOGE(TAG, "manifest has no update procedure");
            r = ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
    }
    if (r == ESP_OK && ctx->man.payload_fetch.present) {
        suit_span_t s = ctx->man.payload_fetch.bytes;
        r = run_top(ex, &s, false);
    }
    if (r == ESP_OK && ctx->man.install.present) {
        suit_span_t s = ctx->man.install.bytes;
        r = run_top(ex, &s, false);
    }
    free(ex);
    return r;
}

esp_err_t suit_exec_validate(struct esp_suit_ota_ctx *ctx)
{
    suit_exec_t *ex = calloc(1, sizeof(*ex));
    if (ex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    exec_init(ex, ctx, SUIT_EXEC_VALIDATE);
    esp_err_t r = run_shared(ex);
    if (r == ESP_OK) {
        r = run_top(ex, &ctx->man.validate, false);
    }
    free(ex);
    return r;
}

esp_err_t suit_exec_check_sequence_shape(struct esp_suit_ota_ctx *ctx, const suit_span_t *seq, bool is_shared)
{
    if (seq->ptr == NULL) {
        return ESP_OK;
    }
    suit_cbor_t c;
    suit_cbor_init(&c, seq->ptr, seq->len);
    size_t n;
    if (suit_cbor_get_array(&c, &n) != ESP_OK || (n % SUIT_SEQ_ITEMS_PER_CMD) != 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    for (size_t k = 0; k < n; k += SUIT_SEQ_ITEMS_PER_CMD) {
        int64_t cmd;
        if (suit_cbor_get_int(&c, &cmd) != ESP_OK || suit_cbor_skip(&c) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (cmd < 0 && is_shared) {
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (k == 0 && ctx->num_comp > 1 && cmd != SUIT_CMD_SET_INDEX) {
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
    }
    return suit_cbor_at_end(&c) ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
}
