/*
 * SPDX-License-Identifier: Apache 2.0 License
 *
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_delta_ota.h"
#include "detools.h"

static const char *TAG = "esp_delta_ota";

typedef struct esp_delta_ota_ctx {
    void *user_data;
    union {
        src_read_cb_t read_cb;        /*!< Read Callback */
        src_read_cb_with_user_ctx_t read_cb_with_user_data; /*!< Read Callback with user data */
    };
    union {
        merged_stream_write_cb_with_user_ctx_t write_cb_with_user_data;
        merged_stream_write_cb_t write_cb;
    };
    esp_delta_ota_checkpoint_cb_t checkpoint_cb;
    struct detools_apply_patch_t *apply_patch;
    int src_offset;

    uint8_t *state_buf;
    size_t state_buf_capacity;
    size_t state_buf_offset;

    const uint8_t *state_read_buf;
    size_t state_read_size;
    size_t state_read_offset;
} esp_delta_ota_ctx;

static int esp_delta_ota_state_write_cb(void *arg_p, const void *buf_p, size_t size)
{
    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)arg_p;
    if (!ctx || (!buf_p && size != 0) || size > SIZE_MAX - ctx->state_buf_offset) {
        return -1;
    }

    size_t required = ctx->state_buf_offset + size;
    if (required > ctx->state_buf_capacity) {
        uint8_t *new_buf = realloc(ctx->state_buf, required);
        if (!new_buf) {
            return -1;
        }
        ctx->state_buf = new_buf;
        ctx->state_buf_capacity = required;
    }

    if (size != 0) {
        memcpy(ctx->state_buf + ctx->state_buf_offset, buf_p, size);
    }
    ctx->state_buf_offset = required;
    return 0;
}

static int esp_delta_ota_state_read_cb(void *arg_p, void *buf_p, size_t size)
{
    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)arg_p;
    if (!ctx || (!buf_p && size != 0) || ctx->state_read_offset > ctx->state_read_size ||
            size > ctx->state_read_size - ctx->state_read_offset) {
        return -1;
    }

    if (size != 0) {
        memcpy(buf_p, ctx->state_read_buf + ctx->state_read_offset, size);
    }
    ctx->state_read_offset += size;
    return 0;
}

static int esp_delta_ota_write_cb(void *arg_p, const uint8_t *buf_p, size_t size)
{
    if (size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_delta_ota_ctx *handle = (esp_delta_ota_ctx *)arg_p;
    esp_err_t err = ESP_OK;
    if (!handle->user_data) {
        err = handle->write_cb(buf_p, size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error in write_cb(): %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    } else {
        err = handle->write_cb_with_user_data(buf_p, size, handle->user_data);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error in write_cb_with_user_data(): %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static int esp_delta_ota_read_cb(void *arg_p, uint8_t *buf_p, size_t size)
{
    if (size <= 0 || !arg_p) {
        return -ESP_ERR_INVALID_ARG;
    }
    esp_delta_ota_ctx *handle = (esp_delta_ota_ctx *)arg_p;
    esp_err_t err = ESP_OK;
    if (!handle->user_data) {
        err = handle->read_cb(buf_p, size, handle->src_offset);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error in read_cb(): %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    } else {
        err = handle->read_cb_with_user_data(buf_p, size, handle->src_offset, handle->user_data);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error in read_cb_with_user_data(): %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    }

    handle->src_offset += size;
    return ESP_OK;
}

static int esp_delta_ota_seek_cb(void *arg_p, int offset)
{
    esp_delta_ota_ctx *handle = (esp_delta_ota_ctx *)arg_p;
    handle->src_offset += offset;
    return ESP_OK;
}

esp_delta_ota_handle_t esp_delta_ota_init(esp_delta_ota_cfg_t *cfg)
{
    if (!cfg) {
        return NULL;
    }

    esp_delta_ota_ctx *ctx = calloc(1, sizeof(esp_delta_ota_ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "Unable to allocate memory");
        return NULL;
    }
    ctx->user_data = cfg->user_data;
    ctx->read_cb = cfg->read_cb;
    ctx->write_cb_with_user_data = cfg->write_cb_with_user_data;
    ctx->checkpoint_cb = cfg->checkpoint_cb;
    ctx->apply_patch = calloc(1, sizeof(struct detools_apply_patch_t));
    if (!ctx->apply_patch) {
        ESP_LOGE(TAG, "Unable to allocate memory");
        free(ctx);
        ctx = NULL;
        return NULL;
    }
    int ret = detools_apply_patch_init(ctx->apply_patch, &esp_delta_ota_read_cb, &esp_delta_ota_seek_cb, 0, &esp_delta_ota_write_cb, ctx);
    if (ret < 0) {
        ESP_LOGE(TAG, "Error while initializing delta_ota: %s", detools_error_as_string(ret));
        free(ctx->apply_patch);
        ctx->apply_patch = NULL;
        free(ctx);
        ctx = NULL;
        return NULL;
    }
    return (esp_delta_ota_handle_t)ctx;
}

esp_delta_ota_handle_t esp_delta_ota_resume(esp_delta_ota_cfg_t *cfg,
                                             const esp_delta_ota_state_t *state,
                                             esp_delta_ota_resume_info_t *resume_info)
{
    if (!cfg || !state || !state->data || state->size == 0 || !resume_info) {
        return NULL;
    }

    esp_delta_ota_handle_t handle = esp_delta_ota_init(cfg);
    if (!handle) {
        return NULL;
    }

    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)handle;
    ctx->state_read_buf = (const uint8_t *)state->data;
    ctx->state_read_size = state->size;
    ctx->state_read_offset = 0;
    ctx->src_offset = 0;

    int ret = detools_apply_patch_restore(ctx->apply_patch, &esp_delta_ota_state_read_cb);
    if (ret < 0 || ctx->state_read_offset != ctx->state_read_size) {
        ESP_LOGE(TAG, "Error while restoring delta_ota state: %s",
                 ret < 0 ? detools_error_as_string(ret) : "invalid state size");
        esp_delta_ota_deinit(handle);
        return NULL;
    }

    ctx->state_read_buf = NULL;
    ctx->state_read_size = 0;
    ctx->state_read_offset = 0;

    resume_info->patch_offset = detools_apply_patch_get_patch_offset(ctx->apply_patch);
    resume_info->output_offset = detools_apply_patch_get_to_offset(ctx->apply_patch);
    return handle;
}

esp_err_t esp_delta_ota_feed_patch(esp_delta_ota_handle_t handle, const uint8_t *buf, int size)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)handle;

    int err = detools_apply_patch_process(ctx->apply_patch, (const uint8_t *)buf, size);
    if (err != 0) {
        ESP_LOGE(TAG, "Error while applying patch: %s", detools_error_as_string(err));
        return ESP_FAIL;
    }

    if (ctx->checkpoint_cb && ctx->apply_patch->state != detools_apply_patch_state_init_t) {
        ctx->state_buf_offset = 0;
        err = detools_apply_patch_dump(ctx->apply_patch, &esp_delta_ota_state_write_cb);
        if (err != 0) {
            ESP_LOGE(TAG, "Error while dumping delta_ota state: %s", detools_error_as_string(err));
            return ESP_FAIL;
        }

        esp_delta_ota_state_t state = {
            .data = ctx->state_buf,
            .size = ctx->state_buf_offset,
        };
        esp_err_t checkpoint_err = ctx->checkpoint_cb(&state, ctx->user_data);
        if (checkpoint_err != ESP_OK) {
            ESP_LOGE(TAG, "Error in checkpoint_cb(): %s", esp_err_to_name(checkpoint_err));
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t esp_delta_ota_finalize(esp_delta_ota_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)handle;

    int err = detools_apply_patch_finalize(ctx->apply_patch);
    if (err < 0) {
        ESP_LOGE(TAG, "Error while finishing the patching: %s", detools_error_as_string(err));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_delta_ota_deinit(esp_delta_ota_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_delta_ota_ctx *ctx = (esp_delta_ota_ctx *)handle;

    free(ctx->state_buf);
    ctx->state_buf = NULL;
    free(ctx->apply_patch);
    ctx->apply_patch = NULL;
    free(ctx);
    ctx = NULL;
    return ESP_OK;
}
