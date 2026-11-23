/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * Where the components of a release go, in two forms.
 *
 * With CONFIG_EXAMPLE_USE_OTA_SLOTS, esp_ota_slots owns the targets. Each one has two slots, the
 * download goes to the spare slot, and activation flips the record. A bad release goes back.
 *
 * Without it, esp_suit_ota uses no slots backend at all. The map names one live partition per
 * component and the download is written straight into it. The app still rolls back, because the
 * bootloader owns that decision, but the data partitions have no previous copy to return to.
 */

#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "example_targets.h"

static const char *TAG = "targets";

#if CONFIG_EXAMPLE_USE_OTA_SLOTS

#include "esp_ota_slots.h"
#include "esp_suit_ota_slots.h"

/*
 * One row per pair: what it is and which partitions it owns.
 *
 * app and f1 flip together (group "cal"). cfg is a COPY pair written to cfg_stage and copied into
 * cfg at activation. The partition labels are free: nothing is derived from the pair name. The
 * manifest reaches these pairs by name, because the last part of a component identifier names its
 * pair, and it chooses the link by the scheme of each component's URI.
 */
static const esp_ota_slots_decl_t s_slots[] = {
    { .name = "app", .kind = ESP_OTA_SLOTS_APP, .group = "cal" },
    { .name = "f1",  .kind = ESP_OTA_SLOTS_AB,  .primary = "f1_live", .secondary = "f1_spare", .group = "cal" },
    { .name = "cfg", .kind = ESP_OTA_SLOTS_COPY, .primary = "cfg",     .secondary = "cfg_stage" },
};

esp_err_t example_targets_init(esp_suit_ota_config_t *cfg)
{
    /* init() also settles a release cut by power loss, or one the bootloader rolled back */
    esp_err_t err = esp_ota_slots_init(s_slots, sizeof(s_slots) / sizeof(s_slots[0]));
    /*
     * A record that exists but cannot be trusted fails init(). Erasing it is the only way back,
     * and it costs the knowledge of which slot is live: every pair restarts at index 0, so a
     * pair whose data sits in index 1 must be written again by the next release. A device that
     * holds data it cannot reproduce must report the fault instead of erasing.
     */
    if (err == ESP_ERR_OTA_SLOTS_RECORD_CORRUPT || err == ESP_ERR_OTA_SLOTS_RECORD_FOREIGN ||
            err == ESP_ERR_OTA_SLOTS_RECORD_VERSION) {
        ESP_LOGE(TAG, "the slots record is unusable (0x%x): erasing it, every pair restarts at index 0", err);
        err = esp_ota_slots_erase_record();
        if (err == ESP_OK) {
            err = esp_ota_slots_init(s_slots, sizeof(s_slots) / sizeof(s_slots[0]));
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_slots_init() failed: 0x%x", err);
        return err;
    }
    cfg->map = NULL;
    cfg->map_len = 0;
    cfg->slots_ops = esp_suit_ota_slots_ops();
    ESP_LOGI(TAG, "A/B slots are on: a bad release goes back to the previous data");
    return ESP_OK;
}

const esp_partition_t *example_live_partition(const char *name)
{
    return esp_ota_slots_get_active(name);
}

void example_confirm_boot(void)
{
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
            st == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
    ESP_ERROR_CHECK(esp_ota_slots_confirm());
}

void example_settle_data(bool data_ok)
{
    if (data_ok) {
        ESP_ERROR_CHECK(esp_ota_slots_confirm());
        ESP_LOGI(TAG, "RESULT: data release confirmed");
    } else {
        ESP_ERROR_CHECK(esp_ota_slots_revert());
        ESP_LOGW(TAG, "RESULT: data release reverted");
    }
}

#else  /* no slots backend: an explicit map names the partitions */

/* the first part of a component identifier is the chip name, so it follows the build target */
static const esp_suit_ota_span_t s_cid_app[2] = {
    ESP_SUIT_OTA_SPAN(CONFIG_IDF_TARGET), ESP_SUIT_OTA_SPAN("app")
};
static const esp_suit_ota_span_t s_cid_f1[2] = {
    ESP_SUIT_OTA_SPAN(CONFIG_IDF_TARGET), ESP_SUIT_OTA_SPAN("f1")
};
static const esp_suit_ota_span_t s_cid_cfg[2] = {
    ESP_SUIT_OTA_SPAN(CONFIG_IDF_TARGET), ESP_SUIT_OTA_SPAN("cfg")
};
static esp_suit_ota_map_entry_t s_map[3];

static const esp_partition_t *data_part(const char *label)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
}

esp_err_t example_targets_init(esp_suit_ota_config_t *cfg)
{
    /* the app goes to the spare OTA slot, because app_update owns that pair whatever we do here */
    s_map[0].id.parts = s_cid_app;
    s_map[0].id.num_parts = 2;
    s_map[0].verify = ESP_SUIT_VERIFY_APP_IMAGE;
    s_map[0].target_partition = esp_ota_get_next_update_partition(NULL);

    /* f1 has no second partition here, so it is written in place and cannot go back */
    s_map[1].id.parts = s_cid_f1;
    s_map[1].id.num_parts = 2;
    s_map[1].verify = ESP_SUIT_VERIFY_DIGEST;
    s_map[1].target_partition = data_part("f1_live");

    /* cfg keeps its staging partition: the download is verified there first, and activate()
     * copies it into the live partition and hashes the copy back. No backend is needed for this. */
    s_map[2].id.parts = s_cid_cfg;
    s_map[2].id.num_parts = 2;
    s_map[2].verify = ESP_SUIT_VERIFY_DIGEST;
    s_map[2].target_partition = data_part("cfg_stage");
    s_map[2].live_partition = data_part("cfg");
    s_map[2].copy_to_live = true;

    for (size_t i = 0; i < 3; i++) {
        if (s_map[i].target_partition == NULL) {
            ESP_LOGE(TAG, "map entry %u has no partition", (unsigned)i);
            return ESP_ERR_NOT_FOUND;
        }
    }
    if (s_map[2].live_partition == NULL) {
        ESP_LOGE(TAG, "no partition labelled 'cfg'");
        return ESP_ERR_NOT_FOUND;
    }
    cfg->map = s_map;
    cfg->map_len = 3;
    cfg->slots_ops = NULL;
    ESP_LOGW(TAG, "A/B slots are off: f1 is overwritten in place, cfg is staged and copied, "
             "and neither can go back");
    return ESP_OK;
}

const esp_partition_t *example_live_partition(const char *name)
{
    /* the live partition of each target, under the name the manifest uses */
    if (strcmp(name, "f1") == 0) {
        return data_part("f1_live");
    }
    return data_part(name);
}

void example_confirm_boot(void)
{
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
            st == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}

void example_settle_data(bool data_ok)
{
    if (data_ok) {
        ESP_LOGI(TAG, "RESULT: data release is live");
    } else {
        ESP_LOGE(TAG, "RESULT: the new data is bad and there is no previous copy to return to");
    }
}

#endif
