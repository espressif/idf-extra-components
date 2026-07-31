/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_schedule.h"
#include "esp_schedule.h"

typedef struct esp_schedule {
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    esp_schedule_trigger_t trigger;
    uint32_t next_scheduled_time_diff;
    esp_schedule_timer_handle_t timer;
    esp_schedule_trigger_cb_t trigger_cb;
    esp_schedule_timestamp_cb_t timestamp_cb;
    void *priv_data;
    esp_schedule_validity_t validity;
} esp_schedule_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Platform access ************************************************************
 *
 * Every call out of this component goes through this one table. It is written
 * once by esp_schedule_port_install() during init and is read-only afterwards,
 * so no locking is needed. install() rejects a config with any required member
 * missing, which is what lets the call sites below dereference these pointers
 * unconditionally instead of NULL-checking on every use. */
extern esp_schedule_port_config_t g_esp_schedule_port;

/* Installs @c port after validating it. Returns ESP_ERR_INVALID_ARG if a
 * required operation is missing, ESP_ERR_INVALID_STATE if already installed. */
esp_err_t esp_schedule_port_install(const esp_schedule_port_config_t *port);

/* True once a port is installed. Public entry points refuse to run before
 * that, because there would be no timer or heap to work with. */
bool esp_schedule_is_inited(void);

#define ESP_SCHEDULE_MALLOC(size)      (g_esp_schedule_port.mem.malloc(size))
#define ESP_SCHEDULE_CALLOC(num, size) (g_esp_schedule_port.mem.calloc((num), (size)))
#define ESP_SCHEDULE_FREE(ptr)         (g_esp_schedule_port.mem.free(ptr))

#define ESP_SCHEDULE_GET_TIME(p_time)  (g_esp_schedule_port.time_sync.get_time(p_time))

/* Logging ********************************************************************
 *
 * Routed through the port so a non-ESP-IDF build never references esp_log.
 * The ceiling below is a compile-time constant, so the comparison folds away
 * and the dead call - including its format string - is dropped by the
 * optimizer. Writing it as a real `if` rather than #if keeps -Wformat checking
 * alive for the levels that are compiled out. */
#if defined(CONFIG_ESP_SCHEDULE_LOG_LEVEL)
#define ESP_SCHEDULE_LOG_CEILING (CONFIG_ESP_SCHEDULE_LOG_LEVEL)
#elif defined(CONFIG_LOG_MAXIMUM_LEVEL)
/* esp_log_level_t counts NONE as 0 and ERROR as 1; ours starts at ERROR. */
#define ESP_SCHEDULE_LOG_CEILING (CONFIG_LOG_MAXIMUM_LEVEL - 1)
#else
#define ESP_SCHEDULE_LOG_CEILING (ESP_SCHEDULE_LOG_INFO)
#endif

void esp_schedule_log(esp_schedule_log_level_t level, const char *tag, const char *format, ...)
__attribute__((format(printf, 3, 4)));

#define ESP_SCHEDULE_LOG_AT(level, tag, format, ...)                   \
    do {                                                               \
        if ((int)(level) <= (int)(ESP_SCHEDULE_LOG_CEILING)) {         \
            esp_schedule_log((level), (tag), (format), ##__VA_ARGS__); \
        }                                                              \
    } while (0)

#define ESP_SCHEDULE_LOGE(tag, format, ...) ESP_SCHEDULE_LOG_AT(ESP_SCHEDULE_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_SCHEDULE_LOGW(tag, format, ...) ESP_SCHEDULE_LOG_AT(ESP_SCHEDULE_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_SCHEDULE_LOGI(tag, format, ...) ESP_SCHEDULE_LOG_AT(ESP_SCHEDULE_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_SCHEDULE_LOGD(tag, format, ...) ESP_SCHEDULE_LOG_AT(ESP_SCHEDULE_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define ESP_SCHEDULE_LOGV(tag, format, ...) ESP_SCHEDULE_LOG_AT(ESP_SCHEDULE_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

/* Storage ********************************************************************/

esp_err_t esp_schedule_nvs_add(esp_schedule_t *schedule);
esp_err_t esp_schedule_nvs_remove(esp_schedule_t *schedule);
esp_err_t esp_schedule_nvs_remove_all(void);
esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count);
bool esp_schedule_nvs_is_enabled(void);
esp_err_t esp_schedule_nvs_init(char *nvs_partition);

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
