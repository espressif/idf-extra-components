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

/* NVS support */
#if defined(CONFIG_ESP_SCHEDULE_ENABLE_NVS) && CONFIG_ESP_SCHEDULE_ENABLE_NVS
#define ESP_SCHEDULE_NVS_ENABLED 1
#else
#define ESP_SCHEDULE_NVS_ENABLED 0
#endif

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

#if ESP_SCHEDULE_NVS_ENABLED
/* On-disk format version for the persisted schedule blob. Bump whenever the
 * layout of esp_schedule_persistent_t or esp_schedule_trigger_t changes in a
 * way that is not backward compatible, so stale blobs are rejected (or
 * migrated) on read. */
#define ESP_SCHEDULE_NVS_FORMAT_VERSION 2

/* Persisted form of a schedule, optionally followed by the application's private
 * data. Runtime-only and runtime-derived fields (live pointers, timer handle,
 * callbacks, and the next-fire countdown which is recomputed on every arm) are
 * intentionally excluded: persisting them is meaningless across reboots and
 * makes the format depend on pointer width and padding.
 *
 * struct_size records sizeof(esp_schedule_persistent_t) as written, so a blob
 * produced by a build with a different layout is rejected rather than
 * misinterpreted. The prime case is CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT being
 * toggled across an OTA, which changes sizeof(esp_schedule_trigger_t).
 *
 * Private data is detected by blob size (buf_size - struct_size), not gated on
 * the version, so a blob written without private data still reads back fine. */
typedef struct esp_schedule_persistent {
    uint8_t version;       /* ESP_SCHEDULE_NVS_FORMAT_VERSION */
    uint16_t struct_size;  /* sizeof(esp_schedule_persistent_t) when written */
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    esp_schedule_trigger_t trigger;
    esp_schedule_validity_t validity;
} esp_schedule_persistent_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_schedule_nvs_add(esp_schedule_t *schedule);
esp_err_t esp_schedule_nvs_remove(esp_schedule_t *schedule);
esp_err_t esp_schedule_nvs_remove_all(void);
esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count);
bool esp_schedule_nvs_is_enabled(void);
esp_err_t esp_schedule_nvs_init(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks);

/* Free private data that the library loaded from NVS via the on_load callback
 * but that never reached the application (e.g. an expired schedule deleted
 * during init). Invokes the registered on_free callback if any; a no-op
 * otherwise. Must not be used on application-owned private data. */
void esp_schedule_nvs_free_loaded_priv_data(void *priv_data);
#endif /* ESP_SCHEDULE_NVS_ENABLED */

/* Returns true if the trigger configuration is valid, i.e. it sets no field its
 * type does not read and no combination without a coherent reading. Logs the
 * offending field(s) on rejection. Exposed for unit testing. */
bool esp_schedule_trigger_is_valid(const esp_schedule_trigger_t *trigger, const char *schedule_name);

/* Year constraint the date arm passes to the date engine: date.year when set,
 * 0 when the pattern recurs every year or fires once, and the current year for a
 * months mask with neither. Exposed for unit testing. */
uint16_t esp_schedule_date_arm_match_year(const esp_schedule_trigger_t *trigger, time_t now);

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
