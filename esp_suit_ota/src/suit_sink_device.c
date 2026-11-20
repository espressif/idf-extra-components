/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "bootloader_common.h"
#include "esp_efuse.h"

static const char *TAG = SUIT_TAG;

esp_err_t suit_sink_part_open(suit_component_t *c, size_t resume_offset)
{
    esp_ota_handle_t h = 0;
    esp_err_t err;
    if (resume_offset > 0) {
        err = esp_ota_resume(c->target.staging, OTA_WITH_SEQUENTIAL_WRITES, resume_offset, &h);
    } else {
        size_t erase = c->target.bulk_erase ? c->size : OTA_WITH_SEQUENTIAL_WRITES;
        err = esp_ota_begin(c->target.staging, erase, &h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin(%s) failed: %s", c->target.staging->label, esp_err_to_name(err));
        return err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE ? ESP_ERR_SUIT_OTA_STATE : err;
    }
    c->ota_handle = h;
    return ESP_OK;
}

esp_err_t suit_sink_part_write(suit_component_t *c, const uint8_t *data, size_t len)
{
    esp_err_t err = esp_ota_write(c->ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write(%s) failed: %s", c->target.staging->label, esp_err_to_name(err));
    }
    return err;
}

esp_err_t suit_sink_part_close(suit_component_t *c)
{
    esp_err_t err = esp_ota_end(c->ota_handle);
    c->ota_handle = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end(%s) failed: %s", c->target.staging->label, esp_err_to_name(err));
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
    return ESP_OK;
}

esp_err_t suit_sink_part_abort(suit_component_t *c)
{
    esp_err_t err = esp_ota_abort(c->ota_handle);
    c->ota_handle = 0;
    return err;
}

static esp_err_t check_app_header(suit_component_t *c)
{
    const size_t desc_off = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    if (c->header_len < desc_off + sizeof(esp_app_desc_t)) {
        ESP_LOGE(TAG, "%s: image shorter than its header", c->id);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
    const esp_image_header_t *hdr = (const esp_image_header_t *)c->header;
    if (hdr->magic != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "%s: not an app image", c->id);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
    if (bootloader_common_check_chip_validity(hdr, ESP_IMAGE_APPLICATION) != ESP_OK) {
        ESP_LOGE(TAG, "%s: chip id or revision mismatch", c->id);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
    const esp_app_desc_t *desc = (const esp_app_desc_t *)(c->header + desc_off);
    if (desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "%s: no app descriptor", c->id);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
    memcpy(&c->app_desc, desc, sizeof(c->app_desc));
    c->has_app_desc = true;
#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    if (!esp_efuse_check_secure_version(desc->secure_version)) {
        ESP_LOGE(TAG, "%s: secure version %lu is below the eFuse value", c->id, (unsigned long)desc->secure_version);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
#endif
#if CONFIG_ESP_SUIT_OTA_VERIFY_SPI_MODE
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_image_header_t cur;
    if (running != NULL && esp_partition_read(running, 0, &cur, sizeof(cur)) == ESP_OK &&
            cur.magic == ESP_IMAGE_HEADER_MAGIC && cur.spi_mode != hdr->spi_mode) {
        ESP_LOGE(TAG, "%s: SPI flash mode %d differs from the running image (%d)", c->id, hdr->spi_mode, cur.spi_mode);
        return ESP_ERR_SUIT_OTA_IMAGE;
    }
#endif
    esp_suit_ota_event_header_t ev = {
        .chip_id = hdr->chip_id,
        .min_chip_rev_full = hdr->min_chip_rev_full,
    };
    memcpy(&ev.app_desc, desc, sizeof(ev.app_desc));
    suit_post_event(ESP_SUIT_OTA_EVENT_COMPONENT_HEADER, &ev, sizeof(ev));
    ESP_LOGI(TAG, "%s: app '%s' version %s", c->id, desc->project_name, desc->version);
    return ESP_OK;
}

esp_err_t suit_sink_check_header(suit_component_t *c)
{
    return (c->target.verify == ESP_SUIT_VERIFY_APP_IMAGE) ? check_app_header(c) : ESP_OK;
}
