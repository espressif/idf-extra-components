/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include "glue_time.h"
#include "glue_log.h"
#include "glue_mem.h"
#include "esp_daylight.h"
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule";

#define SECONDS_TILL_2020 ((2020 - 1970) * 365 * 24 * 3600)
#define SECONDS_IN_DAY (60 * 60 * 24)
#define MINUTES_IN_DAY (60 * 24)

static bool init_done = false;

// Forward declarations for static functions
static void esp_schedule_common_timer_cb(void *priv_data);

/*
 * Unified date-based next occurrence calculation.
 * Returns true and sets *next_time to the next valid time that matches all provided constraints.
 * - now: current time
 * - minutes_since_midnight: target minutes in day [0, 24*60)
 * - days_of_week_mask: bitmask Monday=bit0 .. Sunday=bit6; 0 => any day
 * - day_of_month: 1..31; 0 => any day
 * - months_of_year_mask: bitmask January=bit0 .. December=bit11; 0 => any month
 * - year: 4-digit year (e.g., 2025); 0 => any year
 * - validity: optional window [start,end]; if provided, the returned time will be within this window
 */
bool esp_schedule_get_next_date_time(time_t now,
                                     uint16_t minutes_since_midnight,
                                     uint8_t days_of_week_mask,
                                     uint8_t day_of_month,
                                     uint16_t months_of_year_mask,
                                     uint16_t year,
                                     const esp_schedule_validity_t *validity,
                                     time_t *next_time)
{
    if (next_time == NULL) {
        return false;
    }
    struct tm current_tm = {0};
    localtime_r(&now, &current_tm);
    struct tm candidate_tm = current_tm;

    uint32_t current_seconds_since_midnight = (uint32_t)(current_tm.tm_hour * 3600 + current_tm.tm_min * 60 + current_tm.tm_sec);
    uint32_t target_seconds_since_midnight = (uint32_t)minutes_since_midnight * 60U;

    bool need_next_occurrence = (current_seconds_since_midnight >= target_seconds_since_midnight);

    if (year != 0) {
        int target_year = (int)year - 1900;
        if (current_tm.tm_year > target_year) {
            *next_time = 0;
            return false;
        } else if (current_tm.tm_year < target_year) {
            candidate_tm.tm_year = target_year;
            candidate_tm.tm_mon = 0;
            candidate_tm.tm_mday = 1;
            need_next_occurrence = false;
        }
    }

    time_t candidate_time = mktime(&candidate_tm);
    candidate_tm = *localtime(&candidate_time);

    for (int month_attempts = 0; month_attempts < 25; month_attempts++) {
        bool month_valid = true;
        if (months_of_year_mask != 0) {
            uint16_t month_bit = (uint16_t)(1U << candidate_tm.tm_mon);
            month_valid = (month_bit & months_of_year_mask) != 0;
        }

        if (!month_valid) {
            do {
                candidate_tm.tm_mon++;
                if (candidate_tm.tm_mon >= 12) {
                    candidate_tm.tm_mon = 0;
                    candidate_tm.tm_year++;
                }
                if (year != 0 && candidate_tm.tm_year > ((int)year - 1900)) {
                    *next_time = 0;
                    return false;
                }
                uint16_t month_bit = (uint16_t)(1U << candidate_tm.tm_mon);
                month_valid = (month_bit & months_of_year_mask) != 0;
            } while (!month_valid);

            candidate_tm.tm_mday = 1;
            candidate_time = mktime(&candidate_tm);
            candidate_tm = *localtime(&candidate_time);
            need_next_occurrence = false;
        }

        int days_in_month = 31; /* bounded by normalization */
        for (int day_attempts = 0; day_attempts < days_in_month; day_attempts++) {
            bool day_matches = true;
            if (days_of_week_mask != 0 || day_of_month != 0) {
                day_matches = false;
                if (days_of_week_mask != 0) {
                    uint8_t day_of_week_index = (uint8_t)((candidate_tm.tm_wday + 6) % 7); /* Sunday=0 -> Monday=0 */
                    uint8_t day_bit = (uint8_t)(1U << day_of_week_index);
                    if ((day_bit & days_of_week_mask) != 0) {
                        day_matches = true;
                    }
                }
                if (!day_matches && day_of_month != 0) {
                    if (candidate_tm.tm_mday == day_of_month) {
                        day_matches = true;
                    }
                }
            }

            if (day_matches) {
                if (month_attempts == 0 && day_attempts == 0 && need_next_occurrence) {
                    /* skip today due to time passed */
                } else {
                    candidate_tm.tm_hour = (int)(minutes_since_midnight / 60U);
                    candidate_tm.tm_min = (int)(minutes_since_midnight % 60U);
                    candidate_tm.tm_sec = 0;
                    time_t result_time = mktime(&candidate_tm);

                    if (year != 0) {
                        struct tm check_tm = *localtime(&result_time);
                        if (check_tm.tm_year != ((int)year - 1900)) {
                            *next_time = 0;
                            return false;
                        }
                    }

                    int dst_adjust = 0;
                    if (!current_tm.tm_isdst && candidate_tm.tm_isdst) {
                        dst_adjust = -3600;
                    } else if (current_tm.tm_isdst && !candidate_tm.tm_isdst) {
                        dst_adjust = 3600;
                    }
                    ESP_SCHEDULE_LOGD(TAG, "DST adjust seconds: %d", dst_adjust);
                    result_time += dst_adjust;

                    if (validity && validity->end_time != 0 && result_time > validity->end_time) {
                        *next_time = 0;
                        return false;
                    }
                    if (!validity || validity->start_time == 0 || result_time >= validity->start_time) {
                        *next_time = result_time;
                        return true;
                    }
                    /* else fall through to search next valid day */
                }
            }

            candidate_time += SECONDS_IN_DAY;
            struct tm *next_day_tm = localtime(&candidate_time);
            if (next_day_tm->tm_mon != candidate_tm.tm_mon) {
                candidate_tm = *next_day_tm;
                need_next_occurrence = false;
                break;
            }
            candidate_tm = *next_day_tm;
            need_next_occurrence = false;
            if (year != 0 && candidate_tm.tm_year > ((int)year - 1900)) {
                *next_time = 0;
                return false;
            }
        }
    }

    *next_time = 0;
    return false;
}

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
/**
 * @brief Calculate solar time for a given time in UTC
 * @param is_sunrise: true if sunrise, false if sunset
 * @param time_utc: time in UTC
 * @param latitude: latitude
 * @param longitude: longitude
 * @param offset_minutes: offset in minutes
 * @return solar time in UTC, 0 if calculation failed
 */
time_t esp_schedule_calc_solar_time_for_time_utc(bool is_sunrise, time_t time_utc, double latitude, double longitude, int offset_minutes)
{
    struct tm time_tm = *localtime(&time_utc);
    time_t sunrise_utc, sunset_utc;
    int year = time_tm.tm_year + 1900;
    int month = time_tm.tm_mon + 1;
    int day = time_tm.tm_mday;

    bool calc_ok = esp_daylight_calc_sunrise_sunset_utc(year, month, day, latitude, longitude, &sunrise_utc, &sunset_utc);
    if (!calc_ok) {
        ESP_SCHEDULE_LOGW(TAG, "Failed to calculate %s for date %04d-%02d-%02d at latitude %.5f, longitude %.5f (likely polar night/day condition)",
                          is_sunrise ? "sunrise" : "sunset",
                          year, month, day, latitude, longitude
                         );
        return 0;
    }
    time_t solar_time = is_sunrise ? sunrise_utc : sunset_utc;
    return esp_daylight_apply_offset(solar_time, offset_minutes);
}

time_t esp_schedule_get_next_valid_solar_time(time_t now, const esp_schedule_trigger_t *trigger, const esp_schedule_validity_t *validity, const char *schedule_name)
{
    time_t day_end = 0;
    bool is_sunrise = trigger->type == ESP_SCHEDULE_TYPE_SUNRISE;

    // Find first candidate day (use 23:59 so day selection logic is "date-only")
    if (!esp_schedule_get_next_date_time(now, MINUTES_IN_DAY - 1, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, trigger->date.year, validity, &day_end)) {
        return 0;
    }

    // try for 370 days (max possible days in a year)
    for (int attempts = 0; attempts < 370; attempts++) {
        time_t solar_time = esp_schedule_calc_solar_time_for_time_utc(is_sunrise, day_end, trigger->solar.latitude, trigger->solar.longitude, trigger->solar.offset_minutes);
        if ((solar_time == 0) ||
                (validity && validity->start_time && solar_time < validity->start_time) ||
                (solar_time <= now)) {
            // No solar event on this day (polar conditions) -> advance to next valid day
            // Outside validity window or not in the future -> advance to next valid day
        } else if (validity && validity->end_time && solar_time > validity->end_time) {
            // Past validity window -> return 0
            return 0;
        } else {
            return solar_time;
        }

        // Advance anchor to next day
        if (!esp_schedule_get_next_date_time(day_end + 1, MINUTES_IN_DAY - 1, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, trigger->date.year, validity, &day_end)) {
            return 0;
        }
    }
    return 0;
}
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

/*
 * Ensure trigger->next_scheduled_time_utc is set to the next occurrence.
 * If it's already set in the future, it is reused and not recomputed.
 * Returns true if a valid future time is present after this call, false otherwise.
 */
static bool esp_schedule_set_next_scheduled_time_utc(const char *schedule_name, esp_schedule_trigger_t *trigger, const esp_schedule_validity_t *validity)
{
    struct tm schedule_time;
    time_t now;

    /* Get current time */
    esp_schedule_get_time(&now);
    if (trigger->next_scheduled_time_utc > now) {
        if (validity) {
            if ((validity->start_time && trigger->next_scheduled_time_utc < validity->start_time) ||
                    (validity->end_time && trigger->next_scheduled_time_utc > validity->end_time)) {
                return false;
            }
        }
        return true; /* Already computed and valid */
    }
    /* Handling ESP_SCHEDULE_TYPE_RELATIVE first since it doesn't require any
     * computation based on days, hours, minutes, etc.
     */
    if (trigger->type == ESP_SCHEDULE_TYPE_RELATIVE) {
        /* Compute only once from first encounter. If already set and passed, do not recompute. */
        if (trigger->next_scheduled_time_utc == 0) {
            time_t base = now;
            if (validity && validity->start_time && validity->start_time > now) {
                base = validity->start_time;
            }
            time_t target = base + (time_t)trigger->relative_seconds;
            localtime_r(&target, &schedule_time);
            trigger->next_scheduled_time_utc = mktime(&schedule_time);
        }
        if (validity) {
            if ((validity->start_time && trigger->next_scheduled_time_utc < validity->start_time) ||
                    (validity->end_time && trigger->next_scheduled_time_utc > validity->end_time)) {
                trigger->next_scheduled_time_utc = 0;
                return false;
            }
        }
        return (trigger->next_scheduled_time_utc > now);
    }

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    /* Handle solar-based schedules (sunrise/sunset) */
    if (trigger->type == ESP_SCHEDULE_TYPE_SUNRISE || trigger->type == ESP_SCHEDULE_TYPE_SUNSET) {
        time_t solar_time = esp_schedule_get_next_valid_solar_time(now, trigger, validity, schedule_name);
        if (solar_time == 0) {
            ESP_SCHEDULE_LOGW(TAG, "Solar schedule %s cannot be calculated (no sunrise/sunset at this location/date)", schedule_name);
            return false;
        }

        trigger->next_scheduled_time_utc = solar_time;
        return (trigger->next_scheduled_time_utc > now);
    }
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

    /* Unified DATE and DAYS_OF_WEEK using date finder */
    time_t next_time = 0;
    bool ok = false;
    uint16_t minutes_since_midnight = (uint16_t)(trigger->hours * 60 + trigger->minutes);
    if (trigger->type == ESP_SCHEDULE_TYPE_DATE) {
        ok = esp_schedule_get_next_date_time(now, minutes_since_midnight, 0, trigger->date.day, trigger->date.repeat_months, trigger->date.year, validity, &next_time);
    } else if (trigger->type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        ok = esp_schedule_get_next_date_time(now, minutes_since_midnight, trigger->day.repeat_days, 0, 0, 0, validity, &next_time);
    }
    if (!ok || next_time == 0) {
        return false;
    }
    trigger->next_scheduled_time_utc = next_time;
    return (trigger->next_scheduled_time_utc > now);
}

/*
 * Calculate next time diff across all triggers and set schedule->trigger to the chosen one.
 * Follows current behavior: if all fail (diff 0), returns 0 and clears chosen timestamp.
 */
static uint32_t esp_schedule_get_next_schedule_time_diff_multi(esp_schedule_t *schedule)
{
    if (schedule == NULL) {
        return 0;
    }

    const char *schedule_name = schedule->name;

    /* If no trigger list provided, nothing to schedule */
    if (schedule->triggers.list == NULL || schedule->triggers.count == 0) {
        schedule->next_scheduled_time_utc = 0;
        return 0;
    }

    uint32_t best_diff = 0;
    bool best_set = false;
    time_t best_utc = 0;
    time_t now = 0;
    esp_schedule_get_time(&now);

    for (uint8_t i = 0; i < schedule->triggers.count; i++) {
        esp_schedule_trigger_t *tr = &schedule->triggers.list[i];
        /* Compute or reuse next_scheduled_time_utc */
        bool ok = esp_schedule_set_next_scheduled_time_utc(schedule_name, tr, &schedule->validity);
        if (!ok) {
            continue;
        }
        /* Select nearest future ts */
        if (tr->next_scheduled_time_utc > now) {
            uint32_t diff = (uint32_t)difftime(tr->next_scheduled_time_utc, now);
            if (!best_set || diff < best_diff) {
                best_set = true;
                best_diff = diff;
                best_utc = tr->next_scheduled_time_utc;
            }
        }
    }

    if (!best_set) {
        /* No valid trigger */
        schedule->next_scheduled_time_utc = 0;
        return 0;
    }

    /* Store the chosen timestamp */
    schedule->next_scheduled_time_utc = best_utc;

    /* Print chosen schedule time once */
    {
        char time_str[64];
        struct tm schedule_time;
        memset(&schedule_time, 0, sizeof(schedule_time));
        localtime_r(&best_utc, &schedule_time);
        memset(time_str, 0, sizeof(time_str));
        strftime(time_str, sizeof(time_str), "%c %z[%Z]", &schedule_time);
        ESP_SCHEDULE_LOGI(TAG, "Schedule %s will be active on: %s. DST: %s", schedule_name, time_str, schedule_time.tm_isdst ? "Yes" : "No");
    }
    return best_diff;
}

static void esp_schedule_stop_timer(esp_schedule_t *schedule)
{
    esp_schedule_timer_stop(schedule->timer);
}

static void esp_schedule_start_timer(esp_schedule_t *schedule)
{
    time_t current_time = 0;
    esp_schedule_get_time(&current_time);
    if (current_time < SECONDS_TILL_2020) {
        ESP_SCHEDULE_LOGE(TAG, "Time is not updated");
        return;
    }

    schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff_multi(schedule);

    /* Check if schedule calculation failed (returns 0) */
    if (schedule->next_scheduled_time_diff == 0) {
        ESP_SCHEDULE_LOGW(TAG, "Schedule %s calculation failed or returned invalid time. Skipping timer creation.", schedule->name);
        /* Reset timestamp to indicate schedule is not active */
        schedule->next_scheduled_time_utc = 0;
        return;
    }

    ESP_SCHEDULE_LOGI(TAG, "Starting a timer for %"PRIu32" seconds for schedule %s", schedule->next_scheduled_time_diff, schedule->name);

    if (schedule->timestamp_cb) {
        schedule->timestamp_cb((esp_schedule_handle_t)schedule, (uint32_t)schedule->next_scheduled_time_utc, schedule->priv_data);
    }

    if (schedule->timer == NULL) {
        esp_schedule_timer_start(&schedule->timer, schedule->next_scheduled_time_diff, esp_schedule_common_timer_cb, (void *)schedule);
    } else {
        esp_schedule_timer_reset(schedule->timer, schedule->next_scheduled_time_diff);
    }
}

static void esp_schedule_common_timer_cb(void *priv_data)
{
    esp_schedule_t *schedule = (esp_schedule_t *)priv_data;
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s triggered", schedule->name);
    if (schedule->trigger_cb) {
        schedule->trigger_cb((esp_schedule_handle_t)schedule, schedule->priv_data);
    }
    esp_schedule_start_timer(schedule);
}

static void esp_schedule_delete_timer(esp_schedule_t *schedule)
{
    esp_schedule_timer_cancel(&schedule->timer);
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;

    /* Copy trigger list */
    size_t trigger_list_size = schedule->triggers.count * sizeof(esp_schedule_trigger_t);
    schedule_config->triggers.list = (esp_schedule_trigger_t *)ESP_SCHEDULE_MALLOC(trigger_list_size);
    if (schedule_config->triggers.list == NULL) {
        return ESP_SCHEDULE_RET_NO_MEM;
    }
    memcpy(schedule_config->triggers.list, schedule->triggers.list, trigger_list_size);
    schedule_config->triggers.count = schedule->triggers.count;

    /* Copy name */
    strlcpy(schedule_config->name, schedule->name, sizeof(schedule_config->name));
    schedule_config->trigger_cb = schedule->trigger_cb;
    schedule_config->timestamp_cb = schedule->timestamp_cb;
    schedule_config->priv_data = schedule->priv_data;
    schedule_config->validity = schedule->validity;
    return ESP_SCHEDULE_RET_OK;
}

void esp_schedule_config_free_internals(esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return;
    }
    if (schedule_config->triggers.list) {
        ESP_SCHEDULE_FREE(schedule_config->triggers.list);
        schedule_config->triggers.list = NULL;
        schedule_config->triggers.count = 0;
    }
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_enable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_start_timer(schedule);
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_disable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_stop_timer(schedule);
    /* Disabling a schedule should also reset the next_scheduled_time.
     * It would be re-computed after enabling.
     */
    schedule->next_scheduled_time_utc = 0;
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_reset_trigger_timestamps(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    for (uint8_t i = 0; i < schedule->triggers.count; i++) {
        esp_schedule_trigger_t *tr = &schedule->triggers.list[i];
        if (tr->type != ESP_SCHEDULE_TYPE_RELATIVE) {
            /* Reset the next scheduled time UTC to 0 so that it will be recalculated */
            tr->next_scheduled_time_utc = 0;
        }
    }
    return ESP_SCHEDULE_RET_OK;
}

static ESP_SCHEDULE_RETURN_TYPE esp_schedule_set(esp_schedule_t *schedule, esp_schedule_config_t *schedule_config)
{
    /* Deep copy trigger list from config, freeing previous if any. */
    if (schedule->triggers.list) {
        ESP_SCHEDULE_FREE(schedule->triggers.list);
        schedule->triggers.list = NULL;
        schedule->triggers.count = 0;
    }
    if (schedule_config->triggers.count > 0 && schedule_config->triggers.list != NULL) {
        size_t bytes = (size_t)schedule_config->triggers.count * sizeof(esp_schedule_trigger_t);
        if (schedule->triggers.list) {
            ESP_SCHEDULE_FREE(schedule->triggers.list);
        }
        esp_schedule_trigger_t *copy = (esp_schedule_trigger_t *)ESP_SCHEDULE_CALLOC(1, bytes);
        if (copy == NULL) {
            return ESP_SCHEDULE_RET_NO_MEM;
        }
        memcpy(copy, schedule_config->triggers.list, bytes);
        schedule->triggers.list = copy;
        schedule->triggers.count = schedule_config->triggers.count;

        /* Calculate trigger timestamps once */
        for (uint8_t i = 0; i < schedule->triggers.count; i++) {
            esp_schedule_trigger_t *tr = &schedule->triggers.list[i];
            esp_schedule_set_next_scheduled_time_utc(schedule->name, tr, &schedule_config->validity);
        }
    }

    /* Reset effective timestamp; will be chosen during start */
    schedule->next_scheduled_time_utc = 0;

    schedule->trigger_cb = schedule_config->trigger_cb;
    schedule->timestamp_cb = schedule_config->timestamp_cb;
    schedule->priv_data = schedule_config->priv_data;
    schedule->validity = schedule_config->validity;

#if ESP_SCHEDULE_NVS_ENABLED
    esp_schedule_nvs_add(schedule);
#endif
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (handle == NULL || schedule_config == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    if (strncmp(schedule->name, schedule_config->name, sizeof(schedule->name)) != 0) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule name mismatch. Expected: %s, Passed: %s", schedule->name, schedule_config->name);
        return ESP_SCHEDULE_RET_FAIL;
    }

    /* Reset chosen trigger timestamp; it will be recomputed */
    schedule->next_scheduled_time_utc = 0;
    esp_schedule_set(schedule, schedule_config);
    ESP_SCHEDULE_LOGD(TAG, "Schedule %s edited", schedule->name);
    return ESP_SCHEDULE_RET_OK;
}

static void esp_schedule_free_schedule(esp_schedule_t *schedule)
{
    if (schedule == NULL) {
        return;
    }
    if (schedule->timer) {
        esp_schedule_stop_timer(schedule);
        esp_schedule_delete_timer(schedule);
    }
    if (schedule->triggers.list) {
        ESP_SCHEDULE_FREE(schedule->triggers.list);
        schedule->triggers.list = NULL;
        schedule->triggers.count = 0;
    }
    ESP_SCHEDULE_FREE(schedule);
}

static void esp_schedule_free_all_schedules(esp_schedule_handle_t *handle_list, uint8_t schedule_count)
{
    for (uint8_t i = 0; i < schedule_count; i++) {
        esp_schedule_free_schedule(handle_list[i]);
    }
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_delete(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    ESP_SCHEDULE_LOGI(TAG, "Deleting schedule %s", schedule->name);
#if ESP_SCHEDULE_NVS_ENABLED
    esp_schedule_nvs_remove(schedule);
#endif
    esp_schedule_free_schedule(schedule);
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_delete_all(esp_schedule_handle_t *handle_list, uint8_t schedule_count)
{
#if ESP_SCHEDULE_NVS_ENABLED
    esp_schedule_nvs_remove_all();
#endif
    esp_schedule_free_all_schedules(handle_list, schedule_count);
    return ESP_SCHEDULE_RET_OK;
}

#if ESP_SCHEDULE_NVS_ENABLED
ESP_SCHEDULE_RETURN_TYPE esp_schedule_unload(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    ESP_SCHEDULE_LOGI(TAG, "Freeing schedule %s from memory", schedule->name);
    esp_schedule_free_schedule(schedule);
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_unload_all(esp_schedule_handle_t *handle_list, uint8_t schedule_count)
{
    esp_schedule_free_all_schedules(handle_list, schedule_count);
    return ESP_SCHEDULE_RET_OK;
}
#endif /* ESP_SCHEDULE_NVS_ENABLED */

ESP_SCHEDULE_RETURN_TYPE esp_schedule_create(const esp_schedule_config_t *schedule_config, esp_schedule_handle_t *handle_out)
{
    if (schedule_config == NULL || handle_out == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    if (strlen(schedule_config->name) <= 0) {
        ESP_SCHEDULE_LOGE(TAG, "Set schedule failed. Please enter a unique valid name for the schedule.");
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }

    /* Validate at least one trigger present */
    if (schedule_config->triggers.count == 0 || schedule_config->triggers.list == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule type is invalid.");
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }

    esp_schedule_t *schedule = (esp_schedule_t *)ESP_SCHEDULE_CALLOC(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate handle");
        return ESP_SCHEDULE_RET_NO_MEM;
    }
    strlcpy(schedule->name, schedule_config->name, sizeof(schedule->name));

    ESP_SCHEDULE_RETURN_TYPE ret = esp_schedule_set(schedule, (esp_schedule_config_t *)schedule_config);
    if (ret != ESP_SCHEDULE_RET_OK) {
        ESP_SCHEDULE_FREE(schedule);
        return ret;
    }

    *handle_out = (esp_schedule_handle_t)schedule;
    ESP_SCHEDULE_LOGD(TAG, "Schedule %s created (default)", schedule->name);
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_init_default(void)
{
    esp_schedule_timesync_init();
    init_done = true;
    return ESP_SCHEDULE_RET_OK;
}

#if ESP_SCHEDULE_NVS_ENABLED
/* Returns true only if all triggers are expired per current behavior */
static bool esp_schedule_is_expired(esp_schedule_t *schedule)
{
    for (uint8_t i = 0; i < schedule->triggers.count; i++) {
        if (esp_schedule_set_next_scheduled_time_utc(schedule->name, &schedule->triggers.list[i], &schedule->validity)) {
            return false; // If any trigger is set, the schedule is not expired
        }
    }
    return true; // If all triggers are expired, the schedule is expired
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_init_nvs(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks, uint8_t *schedule_count, esp_schedule_handle_t **handles_out)
{
    if (schedule_count == NULL || handles_out == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }

    esp_schedule_timesync_init();

    /* Initialize NVS */
    ESP_SCHEDULE_RETURN_TYPE ret = esp_schedule_nvs_init(nvs_partition, priv_data_callbacks);
    if (ret != ESP_SCHEDULE_RET_OK) {
        return ret;
    }

    /* Get handle list from NVS */
    *handles_out = esp_schedule_nvs_get_all(schedule_count);
    if (*handles_out == NULL) {
        ESP_SCHEDULE_LOGI(TAG, "No schedules found in NVS");
        *schedule_count = 0;
    } else {
        ESP_SCHEDULE_LOGI(TAG, "Schedules found in NVS: %"PRIu8, *schedule_count);
        /* Start/Delete the schedules */
        esp_schedule_t *schedule = NULL;
        for (int handle_count = *schedule_count - 1; handle_count >= 0; handle_count--) {
            schedule = (esp_schedule_t *) * (*handles_out + handle_count);
            schedule->trigger_cb = NULL;
            schedule->timestamp_cb = NULL;
            schedule->timer = NULL;
            /* Check for ONCE and expired schedules and delete them. */
            if (esp_schedule_is_expired(schedule)) {
                /* This schedule has already expired. */
                ESP_SCHEDULE_LOGI(TAG, "Schedule %s does not repeat and has already expired. Deleting it.", schedule->name);
                esp_schedule_delete((esp_schedule_handle_t)schedule);
                /* Removing the schedule from the list */
                for (int i = handle_count; i < *schedule_count - 2; i++) {
                    (*handles_out)[i] = (*handles_out)[i + 1];
                }
                (*handles_out)[*schedule_count - 1] = NULL;
                (*schedule_count)--;
                continue;
            }
            esp_schedule_start_timer(schedule);
        }
    }
    init_done = true;
    return ESP_SCHEDULE_RET_OK;
}
#endif

esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count)
{
#if !ESP_SCHEDULE_NVS_ENABLED
    // force disable NVS if it is not enabled
    enable_nvs = false;
#endif

    /* Handle default initialization */
    if (!enable_nvs) {
        esp_schedule_init_default();
        *schedule_count = 0;
        return NULL;
    }

    /* Handle NVS initialization */
#if ESP_SCHEDULE_NVS_ENABLED
    esp_schedule_handle_t *handle_list = NULL;
    esp_schedule_init_nvs(nvs_partition, NULL, schedule_count, &handle_list);
    return handle_list;
#else
    // should not happen
    return NULL;
#endif
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_set_trigger_callback(esp_schedule_handle_t handle, esp_schedule_trigger_cb_t trigger_cb)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    schedule->trigger_cb = trigger_cb;
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_set_timestamp_callback(esp_schedule_handle_t handle, esp_schedule_timestamp_cb_t timestamp_cb)
{
    if (handle == NULL) {
        return ESP_SCHEDULE_RET_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    schedule->timestamp_cb = timestamp_cb;
    return ESP_SCHEDULE_RET_OK;
}
