/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_schedule.h"
#include "esp_heap_caps.h"

/** Memory allocation macros for external RAM */
#if ((CONFIG_SPIRAM || CONFIG_SPIRAM_SUPPORT) && \
        (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
#define MEM_ALLOC_EXTRAM(size)         heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_CALLOC_EXTRAM(num, size)   heap_caps_calloc_prefer(num, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_REALLOC_EXTRAM(ptr, size)  heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#else
#define MEM_ALLOC_EXTRAM(size)         malloc(size)
#define MEM_CALLOC_EXTRAM(num, size)   calloc(num, size)
#define MEM_REALLOC_EXTRAM(ptr, size)  realloc(ptr, size)
#endif

typedef struct esp_schedule {
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    esp_schedule_trigger_t trigger;
    uint32_t next_scheduled_time_diff;
    TimerHandle_t timer;
    esp_schedule_trigger_cb_t trigger_cb;
    esp_schedule_timestamp_cb_t timestamp_cb;
    void *priv_data;
    esp_schedule_validity_t validity;
} esp_schedule_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_schedule_nvs_add(esp_schedule_t *schedule);
esp_err_t esp_schedule_nvs_remove(esp_schedule_t *schedule);
esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count);
bool esp_schedule_nvs_is_enabled(void);
esp_err_t esp_schedule_nvs_init(char *nvs_partition);

/* Returns true if a one-shot trigger has already fired and must not be
 * recomputed to a future occurrence. Exposed for unit testing. */
bool esp_schedule_trigger_fired_and_done(const esp_schedule_trigger_t *trigger, time_t now);

/* Unified date-based next occurrence calculation. Returns true and sets
 * *next_time to the next valid time matching all provided constraints.
 * Shared across implementation files and exposed for unit testing. */
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
