/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_daylight.h"
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule";

#define SECONDS_TILL_2020 ((2020 - 1970) * 365 * 24 * 3600)
#define MINUTES_IN_DAY (60 * 24)

static bool init_done = false;

// Forward declarations for static functions
static void esp_schedule_common_timer_cb(TimerHandle_t timer);

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
    /* If the validity window opens in the future, start the search there rather
     * than walking day-by-day from now. The month-attempt cap (~25 months) would
     * otherwise be exhausted before reaching a far-future start_time, causing the
     * schedule to silently never fire. We evaluate the first candidate day
     * unconditionally (force_include_first) and let the exact-instant
     * ">= start_time" validity check below decide whether it qualifies. That
     * avoids seeding need_next_occurrence from a wall-clock-of-day comparison,
     * which could skip the first valid day across a DST transition at the
     * window boundary. */
    bool force_include_first = false;
    if (validity != NULL && validity->start_time > now) {
        now = validity->start_time;
        force_include_first = true;
    }
    struct tm current_tm = {0};
    localtime_r(&now, &current_tm);
    struct tm candidate_tm = current_tm;

    uint32_t current_seconds_since_midnight = (uint32_t)(current_tm.tm_hour * 3600 + current_tm.tm_min * 60 + current_tm.tm_sec);
    uint32_t target_seconds_since_midnight = (uint32_t)minutes_since_midnight * 60U;

    bool need_next_occurrence = (current_seconds_since_midnight >= target_seconds_since_midnight);
    if (force_include_first) {
        /* Do not skip the window-open day; the ">= start_time" check gates it. */
        need_next_occurrence = false;
    }

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

    candidate_tm.tm_isdst = -1;
    time_t candidate_time = mktime(&candidate_tm);
    localtime_r(&candidate_time, &candidate_tm);

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
            candidate_tm.tm_isdst = -1;
            candidate_time = mktime(&candidate_tm);
            localtime_r(&candidate_time, &candidate_tm);
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
                    /* Let mktime resolve DST for the target local time. It uses
                     * tm_isdst to pick the correct epoch, so no manual +/-3600
                     * correction is needed (and applying one double-corrects). */
                    candidate_tm.tm_isdst = -1;
                    time_t result_time = mktime(&candidate_tm);

                    if (year != 0) {
                        struct tm check_tm;
                        localtime_r(&result_time, &check_tm);
                        if (check_tm.tm_year != ((int)year - 1900)) {
                            *next_time = 0;
                            return false;
                        }
                    }

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

            /* Advance to the next calendar day. Incrementing tm_mday and
             * re-normalizing via mktime is DST-safe; adding a fixed 86400
             * seconds skips or repeats a day across DST transitions. */
            int prev_mon = candidate_tm.tm_mon;
            candidate_tm.tm_mday++;
            candidate_tm.tm_isdst = -1;
            candidate_time = mktime(&candidate_tm);
            localtime_r(&candidate_time, &candidate_tm);
            need_next_occurrence = false;
            if (candidate_tm.tm_mon != prev_mon) {
                break;
            }
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
 * @brief Calculate solar time for the calendar date of a given UTC timestamp
 *
 * The calendar date (year/month/day) is derived from @p time_utc in *local*
 * time (via localtime_r), by design: scheduling is wall-clock based, so the
 * solar event is computed for the local date. The returned solar time is UTC.
 *
 * @param is_sunrise: true if sunrise, false if sunset
 * @param time_utc: UTC timestamp whose local calendar date is used
 * @param latitude: latitude
 * @param longitude: longitude
 * @param offset_minutes: offset in minutes
 * @return solar time in UTC, 0 if calculation failed
 */
time_t esp_schedule_calc_solar_time_for_time_utc(bool is_sunrise, time_t time_utc, double latitude, double longitude, int offset_minutes)
{
    struct tm time_tm;
    localtime_r(&time_utc, &time_tm);
    time_t sunrise_utc, sunset_utc;
    int year = time_tm.tm_year + 1900;
    int month = time_tm.tm_mon + 1;
    int day = time_tm.tm_mday;

    bool calc_ok = esp_daylight_calc_sunrise_sunset_utc(year, month, day, latitude, longitude, &sunrise_utc, &sunset_utc);
    if (!calc_ok) {
        ESP_LOGW(TAG, "Failed to calculate %s for date %04d-%02d-%02d at latitude %.5f, longitude %.5f (likely polar night/day condition)",
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

    /* Day selection is identical to a DATE trigger; only the time-of-day is
     * replaced by the computed solar event. repeat_every_year zeroes the year so
     * the month/day pattern recurs, exactly as for DATE. */
    uint16_t match_year = trigger->date.repeat_every_year ? 0 : trigger->date.year;

    // Find first candidate day (use 23:59 so day selection logic is "date-only")
    if (!esp_schedule_get_next_date_time(now, MINUTES_IN_DAY - 1, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, match_year, validity, &day_end)) {
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
        if (!esp_schedule_get_next_date_time(day_end + 1, MINUTES_IN_DAY - 1, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, match_year, validity, &day_end)) {
            return 0;
        }
    }
    return 0;
}
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

/*
 * Returns true if this is a one-shot trigger that has already fired, and must
 * therefore NOT be recomputed to a future occurrence. A trigger has "fired"
 * once its next_scheduled_time_utc is set (>0) and has passed (<=now).
 *
 * Repeating triggers (weekly day-of-week, yearly dates, repeating solar) return
 * false so they are recomputed and re-armed. Without this guard the date engine
 * treats an empty day/month mask as "any", so a DAY_ONCE schedule would re-fire
 * every day and a one-time DATE schedule every month.
 */
bool esp_schedule_trigger_fired_and_done(const esp_schedule_trigger_t *trigger, time_t now)
{
    if (!(trigger->next_scheduled_time_utc > 0 && trigger->next_scheduled_time_utc <= now)) {
        return false; /* not computed yet, or still in the future */
    }
    switch (trigger->type) {
    case ESP_SCHEDULE_TYPE_RELATIVE:
        return true; /* relative schedules fire exactly once */
    case ESP_SCHEDULE_TYPE_DAYS_OF_WEEK:
        return trigger->day.repeat_days == ESP_SCHEDULE_DAY_ONCE;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    /* Solar schedules share DATE's one-shot semantics; only the time-of-day is
     * replaced by the computed sunrise/sunset instant. */
    case ESP_SCHEDULE_TYPE_SUNRISE:
    case ESP_SCHEDULE_TYPE_SUNSET:
#endif
    case ESP_SCHEDULE_TYPE_DATE:
        /* Yearly-repeating dates recur. A date bound to a specific year is
         * bounded by the date engine itself (no match past that year), so it is
         * only genuinely one-shot when no year is set. */
        if (trigger->date.repeat_every_year) {
            return false;
        }
        /* A months mask keeps the schedule firing through the remaining masked
         * months (matching v1 is_expired behavior: e.g. "16th of Jun+Jul+Aug"
         * fires each of those months before expiring), so it is a true one-shot
         * only when no months are set. A specific year is bounded by the engine. */
        if (trigger->date.repeat_months != 0) {
            return false;
        }
        return trigger->date.year == 0;
    default:
        return true;
    }
}

/*
 * Ensure trigger->next_scheduled_time_utc is set to the next occurrence.
 * Repeating date/day-of-week/solar triggers are always recomputed on every arm
 * (so a timezone change is picked up); RELATIVE triggers keep their computed
 * absolute target; one-shot triggers that already fired are left untouched.
 * Returns true if a valid future time is present after this call, false otherwise.
 */
static bool esp_schedule_set_next_scheduled_time_utc(const char *schedule_name, esp_schedule_trigger_t *trigger, const esp_schedule_validity_t *validity)
{
    struct tm schedule_time;
    time_t now;

    /* Get current time */
    time(&now);
    /* Always recompute the next occurrence for repeating date/day-of-week/solar
     * triggers instead of reusing a stored next_scheduled_time_utc. This keeps
     * the fire time correct after a timezone change (picked up on the next arm)
     * without needing an explicit recalculation API. RELATIVE triggers keep
     * their computed absolute target (handled below); one-shot triggers that
     * already fired are guarded next. */
    /* One-shot triggers that have already fired must not be recomputed to a
     * future occurrence (see esp_schedule_trigger_fired_and_done). */
    if (esp_schedule_trigger_fired_and_done(trigger, now)) {
        return false;
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
            ESP_LOGW(TAG, "Solar schedule %s cannot be calculated (no sunrise/sunset at this location/date)", schedule_name);
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
        /* repeat_every_year: ignore the specific year so the month/day pattern
         * recurs every year. Otherwise honor the year, which bounds the schedule
         * to a single year (the engine returns no match once that year passes). */
        uint16_t match_year = trigger->date.repeat_every_year ? 0 : trigger->date.year;
        /* Pass day.repeat_days so a DATE trigger can combine a day-of-week
         * pattern with a day-of-month (the engine ORs the two). */
        ok = esp_schedule_get_next_date_time(now, minutes_since_midnight, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, match_year, validity, &next_time);
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
 * Compute the seconds until the next occurrence of this schedule's single
 * trigger, updating trigger->next_scheduled_time_utc. Returns 0 if there is no
 * valid future occurrence (expired, disabled, or calculation failed).
 */
static uint32_t esp_schedule_get_next_schedule_time_diff(esp_schedule_t *schedule)
{
    time_t now;
    time(&now);

    if (!esp_schedule_set_next_scheduled_time_utc(schedule->name, &schedule->trigger, &schedule->validity)) {
        schedule->trigger.next_scheduled_time_utc = 0;
        return 0;
    }
    if (schedule->trigger.next_scheduled_time_utc <= now) {
        return 0;
    }

    /* Print chosen schedule time once */
    char time_str[64];
    struct tm schedule_time;
    localtime_r(&schedule->trigger.next_scheduled_time_utc, &schedule_time);
    memset(time_str, 0, sizeof(time_str));
    strftime(time_str, sizeof(time_str), "%c %z[%Z]", &schedule_time);
    ESP_LOGI(TAG, "Schedule %s will be active on: %s. DST: %s", schedule->name, time_str, schedule_time.tm_isdst ? "Yes" : "No");

    /* Clamp before the uint32_t cast: casting a double outside uint32_t range is
     * undefined behavior. */
    double diff = difftime(schedule->trigger.next_scheduled_time_utc, now);
    if (diff < 0) {
        diff = 0;
    } else if (diff > (double)UINT32_MAX) {
        diff = (double)UINT32_MAX;
    }
    return (uint32_t)diff;
}

static void esp_schedule_stop_timer(esp_schedule_t *schedule)
{
    xTimerStop(schedule->timer, portMAX_DELAY);
}

/*
 * Arm the FreeRTOS software timer for the given number of seconds. The period
 * is computed in 64-bit and clamped so that a large seconds value cannot
 * overflow the 32-bit tick math ((seconds * 1000) used to overflow for diffs
 * beyond ~49 days). If the requested delay exceeds what a single TickType_t
 * period can represent, it is clamped; esp_schedule_common_timer_cb re-arms for
 * the remaining time when it detects an early expiry.
 */
static void esp_schedule_arm_timer(esp_schedule_t *schedule, uint32_t seconds)
{
    uint64_t ticks = ((uint64_t)seconds * 1000ULL) / (uint64_t)portTICK_PERIOD_MS;
    if (ticks == 0) {
        ticks = 1;
    }
    /* portMAX_DELAY is the maximum representable period. Clamp to one below it so
     * it is never confused with the "block forever" sentinel. */
    const uint64_t max_ticks = (uint64_t)portMAX_DELAY - 1;
    if (ticks > max_ticks) {
        ticks = max_ticks;
    }
    xTimerStop(schedule->timer, portMAX_DELAY);
    xTimerChangePeriod(schedule->timer, (TickType_t)ticks, portMAX_DELAY);
}

static void esp_schedule_start_timer(esp_schedule_t *schedule)
{
    time_t current_time = 0;
    time(&current_time);
    if (current_time < SECONDS_TILL_2020) {
        ESP_LOGE(TAG, "Time is not updated");
        /* Time is no longer valid (e.g. RTC lost). Stop any already-armed timer
         * and clear the chosen time so we don't keep firing on a stale diff. It
         * will be recomputed once time is synced and the schedule re-enabled. */
        if (schedule->timer) {
            esp_schedule_stop_timer(schedule);
        }
        schedule->trigger.next_scheduled_time_utc = 0;
        return;
    }

    schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff(schedule);

    /* Check if schedule calculation failed (returns 0) */
    if (schedule->next_scheduled_time_diff == 0) {
        ESP_LOGW(TAG, "Schedule %s calculation failed or returned invalid time. Skipping timer creation.", schedule->name);
        /* Stop any already-armed timer so a stale diff cannot still fire, then
         * reset timestamp to indicate schedule is not active */
        if (schedule->timer) {
            esp_schedule_stop_timer(schedule);
        }
        schedule->trigger.next_scheduled_time_utc = 0;
        return;
    }

    ESP_LOGI(TAG, "Starting a timer for %"PRIu32" seconds for schedule %s", schedule->next_scheduled_time_diff, schedule->name);

    if (schedule->timestamp_cb) {
        schedule->timestamp_cb((esp_schedule_handle_t)schedule, (uint32_t)schedule->trigger.next_scheduled_time_utc, schedule->priv_data);
    }

    esp_schedule_arm_timer(schedule, schedule->next_scheduled_time_diff);
}

static void esp_schedule_common_timer_cb(TimerHandle_t timer)
{
    void *priv_data = pvTimerGetTimerID(timer);
    if (priv_data == NULL) {
        return;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)priv_data;

    /* Guard against a premature timer expiry: if the scheduled instant has not
     * actually arrived (e.g. tick truncation on a very long delay that had to be
     * clamped), re-arm for the remaining time instead of firing early.
     * esp_schedule_start_timer recomputes the diff from the still-future
     * next_scheduled_time_utc, so it re-arms for what remains. */
    time_t now = 0;
    time(&now);
    if (schedule->trigger.next_scheduled_time_utc > now) {
        ESP_LOGW(TAG, "Schedule %s fired early; rescheduling for the remaining time", schedule->name);
        esp_schedule_start_timer(schedule);
        return;
    }

    /* Re-check the validity window at fire time. The occurrence was computed to
     * be within [start,end], but callback dispatch can be delayed past end_time
     * (tick truncation, timer-queue latency, system load). Suppress a trigger
     * that is now outside the window; the re-arm below will find no further
     * valid occurrence and leave the schedule disarmed. */
    if (schedule->validity.end_time != 0 && now > schedule->validity.end_time) {
        ESP_LOGW(TAG, "Schedule %s expired before dispatch; suppressing out-of-window trigger", schedule->name);
        esp_schedule_start_timer(schedule);
        return;
    }

    ESP_LOGI(TAG, "Schedule %s triggered", schedule->name);
    if (schedule->trigger_cb) {
        schedule->trigger_cb((esp_schedule_handle_t)schedule, schedule->priv_data);
    }

    esp_schedule_start_timer(schedule);
}

static void esp_schedule_delete_timer(esp_schedule_t *schedule)
{
    xTimerDelete(schedule->timer, portMAX_DELAY);
}

static void esp_schedule_create_timer(esp_schedule_t *schedule)
{
    if (esp_schedule_nvs_is_enabled()) {
        /* This is used for calculating next_scheduled_time_utc, and only used when NVS is enabled.
         * If NVS is enabled, time will already be synced and the time will be correctly calculated. */
        schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff(schedule);
    }

    /* Temporarily setting the timer for 1 (anything greater than 0) tick. This will get changed when xTimerChangePeriod() is called. */
    schedule->timer = xTimerCreate("schedule", 1, pdFALSE, (void *)schedule, esp_schedule_common_timer_cb);
}

esp_err_t esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;

    strlcpy(schedule_config->name, schedule->name, sizeof(schedule_config->name));
    schedule_config->trigger.type = schedule->trigger.type;
    schedule_config->trigger.hours = schedule->trigger.hours;
    schedule_config->trigger.minutes = schedule->trigger.minutes;
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule_config->trigger.relative_seconds = schedule->trigger.relative_seconds;
        schedule_config->trigger.next_scheduled_time_utc = schedule->trigger.next_scheduled_time_utc;
    } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        schedule_config->trigger.day.repeat_days = schedule->trigger.day.repeat_days;
    } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DATE) {
        schedule_config->trigger.day.repeat_days = schedule->trigger.day.repeat_days;
        schedule_config->trigger.date.day = schedule->trigger.date.day;
        schedule_config->trigger.date.repeat_months = schedule->trigger.date.repeat_months;
        schedule_config->trigger.date.year = schedule->trigger.date.year;
        schedule_config->trigger.date.repeat_every_year = schedule->trigger.date.repeat_every_year;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNRISE || schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNSET) {
        /* Solar carries the same day/date pattern as DATE, plus location. */
        schedule_config->trigger.day.repeat_days = schedule->trigger.day.repeat_days;
        schedule_config->trigger.date.day = schedule->trigger.date.day;
        schedule_config->trigger.date.repeat_months = schedule->trigger.date.repeat_months;
        schedule_config->trigger.date.year = schedule->trigger.date.year;
        schedule_config->trigger.date.repeat_every_year = schedule->trigger.date.repeat_every_year;
        schedule_config->trigger.solar.latitude = schedule->trigger.solar.latitude;
        schedule_config->trigger.solar.longitude = schedule->trigger.solar.longitude;
        schedule_config->trigger.solar.offset_minutes = schedule->trigger.solar.offset_minutes;
#endif
    }

    schedule_config->trigger_cb = schedule->trigger_cb;
    schedule_config->timestamp_cb = schedule->timestamp_cb;
    schedule_config->priv_data = schedule->priv_data;
    schedule_config->validity = schedule->validity;
    return ESP_OK;
}

esp_err_t esp_schedule_enable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_start_timer(schedule);
    return ESP_OK;
}

esp_err_t esp_schedule_disable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_stop_timer(schedule);
    /* Disabling a schedule should also reset the next_scheduled_time.
     * It would be re-computed after enabling.
     */
    schedule->trigger.next_scheduled_time_utc = 0;
    return ESP_OK;
}

static esp_err_t esp_schedule_set(esp_schedule_t *schedule, esp_schedule_config_t *schedule_config)
{
    /* Setting everything apart from name. */
    schedule->trigger.type = schedule_config->trigger.type;
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule->trigger.relative_seconds = schedule_config->trigger.relative_seconds;
        schedule->trigger.next_scheduled_time_utc = schedule_config->trigger.next_scheduled_time_utc;
    } else {
        schedule->trigger.hours = schedule_config->trigger.hours;
        schedule->trigger.minutes = schedule_config->trigger.minutes;

        if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
            schedule->trigger.day.repeat_days = schedule_config->trigger.day.repeat_days;
        } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DATE) {
            schedule->trigger.day.repeat_days = schedule_config->trigger.day.repeat_days;
            schedule->trigger.date.day = schedule_config->trigger.date.day;
            schedule->trigger.date.repeat_months = schedule_config->trigger.date.repeat_months;
            schedule->trigger.date.year = schedule_config->trigger.date.year;
            schedule->trigger.date.repeat_every_year = schedule_config->trigger.date.repeat_every_year;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
        } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNRISE || schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNSET) {
            schedule->trigger.solar.latitude = schedule_config->trigger.solar.latitude;
            schedule->trigger.solar.longitude = schedule_config->trigger.solar.longitude;
            schedule->trigger.solar.offset_minutes = schedule_config->trigger.solar.offset_minutes;
            /* Copy day and date fields for unified solar schedule approach */
            schedule->trigger.day.repeat_days = schedule_config->trigger.day.repeat_days;
            schedule->trigger.date.day = schedule_config->trigger.date.day;
            schedule->trigger.date.repeat_months = schedule_config->trigger.date.repeat_months;
            schedule->trigger.date.year = schedule_config->trigger.date.year;
            schedule->trigger.date.repeat_every_year = schedule_config->trigger.date.repeat_every_year;
#endif
        }
    }

    schedule->trigger_cb = schedule_config->trigger_cb;
    schedule->timestamp_cb = schedule_config->timestamp_cb;
    schedule->priv_data = schedule_config->priv_data;
    schedule->validity = schedule_config->validity;
    esp_schedule_nvs_add(schedule);
    return ESP_OK;
}

esp_err_t esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (handle == NULL || schedule_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    if (strncmp(schedule->name, schedule_config->name, sizeof(schedule->name)) != 0) {
        ESP_LOGE(TAG, "Schedule name mismatch. Expected: %s, Passed: %s", schedule->name, schedule_config->name);
        return ESP_FAIL;
    }

    /* Editing a schedule with relative time should also reset it. */
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule->trigger.next_scheduled_time_utc = 0;
    }
    esp_schedule_set(schedule, schedule_config);
    ESP_LOGD(TAG, "Schedule %s edited", schedule->name);
    return ESP_OK;
}

esp_err_t esp_schedule_delete(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    ESP_LOGI(TAG, "Deleting schedule %s", schedule->name);
    if (schedule->timer) {
        esp_schedule_stop_timer(schedule);
        esp_schedule_delete_timer(schedule);
    }
    esp_schedule_nvs_remove(schedule);
    free(schedule);
    return ESP_OK;
}

esp_schedule_handle_t esp_schedule_create(esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return NULL;
    }
    if (strlen(schedule_config->name) <= 0) {
        ESP_LOGE(TAG, "Set schedule failed. Please enter a unique valid name for the schedule.");
        return NULL;
    }

    if (schedule_config->trigger.type == ESP_SCHEDULE_TYPE_INVALID) {
        ESP_LOGE(TAG, "Schedule type is invalid.");
        return NULL;
    }

    esp_schedule_t *schedule = (esp_schedule_t *)MEM_CALLOC_EXTRAM(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_LOGE(TAG, "Could not allocate handle");
        return NULL;
    }
    strlcpy(schedule->name, schedule_config->name, sizeof(schedule->name));

    esp_schedule_set(schedule, schedule_config);

    esp_schedule_create_timer(schedule);
    ESP_LOGD(TAG, "Schedule %s created", schedule->name);
    return (esp_schedule_handle_t)schedule;
}

esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count)
{
    if (!esp_sntp_enabled()) {
        ESP_LOGI(TAG, "Initializing SNTP");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    }

    if (!enable_nvs) {
        return NULL;
    }

    if (schedule_count == NULL) {
        ESP_LOGE(TAG, "schedule_count cannot be NULL when NVS is enabled");
        return NULL;
    }

    /* Wait for time to be updated here */

    /* Below this is initialising schedules from NVS */
    esp_schedule_nvs_init(nvs_partition);

    /* Get handle list from NVS */
    esp_schedule_handle_t *handle_list = NULL;
    *schedule_count = 0;
    handle_list = esp_schedule_nvs_get_all(schedule_count);
    if (handle_list == NULL) {
        ESP_LOGI(TAG, "No schedules found in NVS");
        return NULL;
    }
    ESP_LOGI(TAG, "Schedules found in NVS: %"PRIu8, *schedule_count);
    /* Start/Delete the schedules */
    esp_schedule_t *schedule = NULL;
    for (size_t handle_count = 0; handle_count < *schedule_count; handle_count++) {
        schedule = (esp_schedule_t *)handle_list[handle_count];
        schedule->trigger_cb = NULL;
        schedule->timestamp_cb = NULL;
        schedule->timer = NULL;
        /* Check for ONCE and expired schedules and delete them. A schedule with
         * no valid future occurrence (already-fired one-shot, or a year bounded
         * in the past) is expired. */
        bool has_future = esp_schedule_set_next_scheduled_time_utc(schedule->name, &schedule->trigger, &schedule->validity);
        if (!has_future) {
            /* This schedule has already expired. */
            ESP_LOGI(TAG, "Schedule %s does not repeat and has already expired. Deleting it.", schedule->name);
            esp_schedule_delete((esp_schedule_handle_t)schedule);
            /* Removing the schedule from the list */
            handle_list[handle_count] = handle_list[*schedule_count - 1];
            (*schedule_count)--;
            handle_count--;
            continue;
        }
        esp_schedule_create_timer(schedule);
        esp_schedule_start_timer(schedule);
    }
    init_done = true;
    return handle_list;
}
