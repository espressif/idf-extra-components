/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "glue_timer.h"
#include "esp_schedule.h"

/* NVS support */
#if defined(CONFIG_ESP_SCHEDULE_ENABLE_NVS) && CONFIG_ESP_SCHEDULE_ENABLE_NVS
#define ESP_SCHEDULE_NVS_ENABLED 1
#else
#define ESP_SCHEDULE_NVS_ENABLED 0
#endif

typedef struct esp_schedule {
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    /* List of triggers associated with this schedule. We deep-copy from config. */
    esp_schedule_trigger_list_t triggers;
    uint32_t next_scheduled_time_diff;
    time_t next_scheduled_time_utc;
    esp_schedule_timer_handle_t timer;
    esp_schedule_trigger_cb_t trigger_cb;
    esp_schedule_timestamp_cb_t timestamp_cb;
    void *priv_data;
    esp_schedule_validity_t validity;
} esp_schedule_t;

#if ESP_SCHEDULE_NVS_ENABLED
/* Persistent part of schedule for NVS storage (excludes runtime pointers) */
typedef struct esp_schedule_persistent {
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    /* List of triggers associated with this schedule. We deep-copy from config. */
    esp_schedule_trigger_list_t triggers;
    uint32_t next_scheduled_time_diff;
    time_t next_scheduled_time_utc;
    esp_schedule_validity_t validity;
} esp_schedule_persistent_t;
#endif /* ESP_SCHEDULE_NVS_ENABLED */

#ifdef __cplusplus
extern "C" {
#endif

#if ESP_SCHEDULE_NVS_ENABLED
ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_add(esp_schedule_t *schedule);
ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_remove(esp_schedule_t *schedule);
ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_remove_all(void);
esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count);
bool esp_schedule_nvs_is_enabled(void);
ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_init(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks);
#endif

/* Internal time calculation helpers (shared across implementation files) */
bool esp_schedule_get_next_date_time(
    time_t now,
    uint16_t minutes_since_midnight,
    uint8_t days_of_week_mask,
    uint8_t day_of_month,
    uint16_t months_of_year_mask,
    uint16_t year,
    const esp_schedule_validity_t *validity,
    time_t *next_time
);

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
time_t esp_schedule_calc_solar_time_for_time_utc(
    bool is_sunrise,
    time_t time_utc,
    double latitude,
    double longitude,
    int offset_minutes
);

time_t esp_schedule_get_next_valid_solar_time(
    time_t now,
    const esp_schedule_trigger_t *trigger,
    const esp_schedule_validity_t *validity,
    const char *schedule_name
);
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

#ifdef __cplusplus
}
#endif