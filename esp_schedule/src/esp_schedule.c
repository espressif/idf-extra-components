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
#include "esp_daylight.h"
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule";

#define SECONDS_TILL_2020 ((2020 - 1970) * 365 * 24 * 3600)
#define MINUTES_IN_DAY (60 * 24)

/* Longest gap between two leap years, in years. Normally 4, but a century that
 * is not a leap year stretches it to 8: 2096 and 2104 are leap, 2100 is not. */
#define MAX_LEAP_YEAR_GAP 8

/*
 * Search horizon of the date engine in esp_schedule_get_next_date_time(),
 * counted in *masked* months - months outside months_of_year_mask are skipped
 * without spending one, so the budget only covers months actually walked day by
 * day.
 *
 * This is the exact bound required, not a padded one:
 *
 *     1  the starting month, which may be partially elapsed (the engine can skip
 *        today when the target time of day has already passed)
 *   + N  the longest run of consecutive masked months in which day_of_month does
 *        not exist
 *   + 1  the month that finally matches
 *
 * Only day_of_month can be missing from a month; a days_of_week_mask always
 * matches, since every month contains all seven weekdays. So N is maximized by
 * the narrowest mask over the rarest day:
 *
 *   day <= 28, or any dow mask   N = 0   present in every month
 *   day 31, no months mask       N = 1   Feb, Apr, Jun, Sep and Nov all lack a
 *                                       31st, but none of them are adjacent
 *   day 29, mask = Feb only      N = 7   <-- worst satisfiable case
 *
 * N = 7 is the run of non-leap Februaries inside the longest leap gap, hence
 * N = MAX_LEAP_YEAR_GAP - 1 and a bound of 9. Worked example, starting late on
 * 29 Feb 2096 with the target time already past: attempt 0 skips today, attempts
 * 1..7 walk Feb 2097..2103, attempt 8 matches 29 Feb 2104. (A 32-bit time_t
 * cannot reach 2096, capping the gap at 4 years there, so this is the 64-bit
 * worst case.)
 *
 * Patterns that can never match - day 30 masked to February, day 31 masked to
 * only 30-day months - have no finite N. They exhaust the horizon and fail
 * regardless of its value, so it merely caps their wasted work.
 *
 * This holds for the current day rule (dow mask OR day_of_month). Adding an
 * "nth weekday of month" rule would need N re-derived, as such a day can be
 * absent from several consecutive months - that calls for a new term in the sum,
 * not a larger MAX_LEAP_YEAR_GAP.
 */
#define MAX_MASKED_MONTH_ATTEMPTS (1 + (MAX_LEAP_YEAR_GAP - 1) + 1)

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
    /* Reject an out-of-range time-of-day instead of letting mktime() normalize it.
     * With minutes_since_midnight >= 24*60 the candidate would roll into the
     * following day, so the returned instant could land on a day that violates
     * the day/month mask this engine just matched. */
    if (minutes_since_midnight >= MINUTES_IN_DAY) {
        *next_time = 0;
        return false;
    }
    /* If the validity window opens in the future, start the search there rather
     * than walking day-by-day from now. The month-attempt cap below would
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

    /* One attempt == one *masked* month, searched day by day below. A month
     * outside months_of_year_mask is skipped by the inner do-while without
     * consuming an attempt, so the horizon is spent only on months actually
     * walked. See MAX_MASKED_MONTH_ATTEMPTS for why that horizon is exact. */
    for (int month_attempts = 0; month_attempts < MAX_MASKED_MONTH_ATTEMPTS; month_attempts++) {
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

/*
 * Year constraint the date arm passes to the C engine. This is where the three
 * recurrence spellings differ:
 *
 *   repeat_every_year  the pattern recurs in every year        -> unconstrained
 *   year = N           bounded to that year                    -> N
 *   neither, with a
 *   months mask        bounded to the *current* year           -> this year
 *
 * The last row is why a months mask alone is not a one-shot: it fires each masked
 * month of the current year and then finds no further match. Because nothing is
 * stored, the bound is re-resolved from `now` on every arm, so a reboot in a
 * later calendar year re-bounds it to that year. Use `year = N` when the end must
 * survive a reboot, or `repeat_every_year` when it should never end.
 *
 * With no months mask there is nothing to iterate, so the trigger fires once and
 * the year is left unconstrained (it may land in the next year).
 */
uint16_t esp_schedule_date_arm_match_year(const esp_schedule_trigger_t *trigger, time_t now)
{
    if (trigger->date.year != 0) {
        return trigger->date.year;
    }
    if (trigger->date.repeat_every_year || trigger->date.repeat_months == 0) {
        return 0;
    }
    struct tm now_tm;
    localtime_r(&now, &now_tm);
    return (uint16_t)(now_tm.tm_year + 1900);
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

    /* Day selection is the day-of-week arm or the date arm, never both (the two
     * are mutually exclusive, enforced by validation); only the time-of-day is
     * replaced by the computed solar event. */
    uint16_t match_year = esp_schedule_date_arm_match_year(trigger, now);

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
            ESP_SCHEDULE_LOGD(TAG, "Schedule %s: next solar event is past validity end_time.", schedule_name);
            return 0;
        } else {
            return solar_time;
        }

        // Advance anchor to next day
        if (!esp_schedule_get_next_date_time(day_end + 1, MINUTES_IN_DAY - 1, trigger->day.repeat_days, trigger->date.day, trigger->date.repeat_months, match_year, validity, &day_end)) {
            ESP_SCHEDULE_LOGD(TAG, "Schedule %s: no further day matches the date/day-of-week arm.", schedule_name);
            return 0;
        }
    }
    ESP_SCHEDULE_LOGD(TAG, "Schedule %s: no solar event found within 370 candidate days.", schedule_name);
    return 0;
}
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

/* True if any field of the date arm is populated. There is no encoding for
 * "field not set", so an all-zero date arm is the wildcard, not "absent". */
static bool esp_schedule_date_arm_present(const esp_schedule_trigger_t *trigger)
{
    return (trigger->date.day != 0 ||
            trigger->date.repeat_months != 0 ||
            trigger->date.year != 0 ||
            trigger->date.repeat_every_year);
}

/*
 * Validate the date arm (rules V3..V6 of docs/trigger_rules.md, evaluated in
 * that order). Reached only when the day-of-week arm is unused.
 */
static bool esp_schedule_date_arm_is_valid(const esp_schedule_trigger_t *trigger, const char *schedule_name)
{
    /* V3: a months mask with no day-of-month would mean every day of those
     * months, which is never the intended schedule. */
    if (trigger->date.day == 0 && trigger->date.repeat_months != 0) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule %s: date.repeat_months is set but date.day is 0. Set date.day.", schedule_name);
        return false;
    }
    /* V4: a recurrence or a year bound with no date pattern to apply it to. */
    if (trigger->date.day == 0 && trigger->date.repeat_months == 0 &&
            (trigger->date.year != 0 || trigger->date.repeat_every_year)) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule %s: date.year/date.repeat_every_year set with no date.day or date.repeat_months to apply it to.", schedule_name);
        return false;
    }
    /* V5: "only year N" and "every year" are contradictory. */
    if (trigger->date.year != 0 && trigger->date.repeat_every_year) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule %s: date.year and date.repeat_every_year are mutually exclusive.", schedule_name);
        return false;
    }
    /* V6: repeat_every_year recurs *over the month set*, so with no mask it
     * would be inert. date.year is exempt: it still constrains the arm. */
    if (trigger->date.repeat_every_year && trigger->date.repeat_months == 0) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule %s: date.repeat_every_year needs date.repeat_months to recur over (use ESP_SCHEDULE_MONTH_ALL for every month).", schedule_name);
        return false;
    }
    return true;
}

/*
 * Validate the trigger configuration. Each type reads only the fields it owns
 * (day.repeat_days for DAYS_OF_WEEK, date.* for DATE, either arm for solar);
 * setting a field the type does not read, or a combination with no coherent
 * reading, is a configuration error rather than a silently discarded value.
 * The rules are tabulated in docs/trigger_rules.md.
 *
 * Rejecting these guarantees two invariants the rest of the code relies on: the
 * day-of-week and day-of-month arms are never both set, and date.year is already
 * 0 whenever date.repeat_every_year is set.
 *
 * Range validation of hours/minutes is separate, see
 * esp_schedule_config_time_of_day_is_valid().
 */
bool esp_schedule_trigger_is_valid(const esp_schedule_trigger_t *trigger, const char *schedule_name)
{
    switch (trigger->type) {
    case ESP_SCHEDULE_TYPE_RELATIVE:
        /* Reads no date field; all of them are ignored. The one field it does own
         * must name a future instant. V7: a non-positive delay puts the target at
         * or before the base time, so the arm path would find nothing to fire and
         * the schedule would silently never run. */
        if (trigger->relative_seconds <= 0) {
            ESP_SCHEDULE_LOGE(TAG, "Schedule %s: relative_seconds must be > 0, got %d.", schedule_name, trigger->relative_seconds);
            return false;
        }
        return true;
    case ESP_SCHEDULE_TYPE_DAYS_OF_WEEK:
        /* V1: this type reads only day.repeat_days. */
        if (esp_schedule_date_arm_present(trigger)) {
            ESP_SCHEDULE_LOGE(TAG, "Schedule %s: DAYS_OF_WEEK reads only day.repeat_days, but a date.* field is set.", schedule_name);
            return false;
        }
        return true;
    case ESP_SCHEDULE_TYPE_DATE:
        /* V2: DATE has no day-of-week arm. */
        if (trigger->day.repeat_days != 0) {
            ESP_SCHEDULE_LOGE(TAG, "Schedule %s: DATE reads only date.*, but day.repeat_days is set. Use DAYS_OF_WEEK instead.", schedule_name);
            return false;
        }
        return esp_schedule_date_arm_is_valid(trigger, schedule_name);
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    case ESP_SCHEDULE_TYPE_SUNRISE:
    /* fall-through */
    case ESP_SCHEDULE_TYPE_SUNSET:
        /* Solar selects one day arm from whichever is populated. V2: both
         * populated is ambiguous, not a union. */
        if (trigger->day.repeat_days != 0) {
            if (esp_schedule_date_arm_present(trigger)) {
                ESP_SCHEDULE_LOGE(TAG, "Schedule %s: solar day.repeat_days and date.* are mutually exclusive arms; only one may be set.", schedule_name);
                return false;
            }
            return true; /* day-of-week arm */
        }
        return esp_schedule_date_arm_is_valid(trigger, schedule_name);
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */
    default:
        return false;
    }
}

/*
 * One-shot rule for the date arm, shared by DATE and by solar when its date arm
 * is the selected one.
 *
 * The months mask gates all date-arm recurrence: every recurrence iterates the
 * month set, so with no mask there is nothing to iterate and the trigger fires
 * exactly once. With a mask it always recurs; how far is decided by the year
 * constraint above, and exhausting it makes the C engine return no match, which
 * the arm path turns into disarm-and-delete.
 */
static bool esp_schedule_date_arm_is_one_shot(const esp_schedule_trigger_t *trigger)
{
    return trigger->date.repeat_months == 0;
}

/*
 * Returns true if this trigger fires exactly once, i.e. it has no recurring
 * pattern that would produce a further occurrence after the current one. This is
 * a property of the configuration alone and says nothing about whether the
 * trigger has already fired (see esp_schedule_trigger_fired_and_done).
 */
static bool esp_schedule_trigger_is_one_shot(const esp_schedule_trigger_t *trigger)
{
    switch (trigger->type) {
    case ESP_SCHEDULE_TYPE_RELATIVE:
        return true; /* relative schedules fire exactly once */
    case ESP_SCHEDULE_TYPE_DAYS_OF_WEEK:
        return trigger->day.repeat_days == ESP_SCHEDULE_DAY_ONCE;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    case ESP_SCHEDULE_TYPE_SUNRISE:
    /* fall-through */
    case ESP_SCHEDULE_TYPE_SUNSET:
        if (trigger->day.repeat_days != 0) {
            return false; /* day-of-week arm: repeats on every set weekday, forever */
        }
        /* Otherwise solar shares DATE's date-arm rule; only the time-of-day is
         * replaced by the computed sunrise/sunset instant. */
        return esp_schedule_date_arm_is_one_shot(trigger);
#endif
    case ESP_SCHEDULE_TYPE_DATE:
        return esp_schedule_date_arm_is_one_shot(trigger);
    default:
        return true;
    }
}

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
    return esp_schedule_trigger_is_one_shot(trigger);
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
    ESP_SCHEDULE_GET_TIME(&now);
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
            /* Kept within ESP_SCHEDULE_LOG_BUF_LEN even with a full-length name;
             * see the buffer comment in esp_schedule_port.c. */
            ESP_SCHEDULE_LOGW(TAG, "Solar schedule %s has no next occurrence: no solar event at this location/date, no matching day left, or past validity. Enable debug logs.", schedule_name);
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
        /* DATE has no day-of-week arm, so 0 is passed for it. The year constraint
         * comes from esp_schedule_date_arm_match_year(). */
        ok = esp_schedule_get_next_date_time(now, minutes_since_midnight, 0, trigger->date.day, trigger->date.repeat_months, esp_schedule_date_arm_match_year(trigger, now), validity, &next_time);
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
    ESP_SCHEDULE_GET_TIME(&now);

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
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s will be active on: %s. DST: %s", schedule->name, time_str, schedule_time.tm_isdst ? "Yes" : "No");

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
    g_esp_schedule_port.timer.stop(schedule->timer);
}

/*
 * Arm the schedule timer for the given number of seconds. The underlying timer is
 * created on the first arm and reused on every later arm. The glue layer owns the
 * conversion to its native period and splits a delay too long to be represented
 * in one period across several of them; esp_schedule_common_timer_cb also re-arms
 * for the remaining time if it ever detects an early expiry.
 */
static void esp_schedule_arm_timer(esp_schedule_t *schedule, uint32_t seconds)
{
    if (!g_esp_schedule_port.timer.start(&schedule->timer, seconds, esp_schedule_common_timer_cb, (void *)schedule)) {
        ESP_SCHEDULE_LOGE(TAG, "Failed to arm timer for schedule %s", schedule->name);
        /* A failed re-arm of an existing timer does not disarm it: start() may
         * have been unable to apply the new period while the old one is still
         * running, and it would then fire on a time we have just discarded.
         * Stop it explicitly, the same way the two bail-outs in
         * esp_schedule_start_timer() do. Best-effort: whatever made start()
         * fail may well make this fail too, but leaving the stale period armed
         * is strictly worse. */
        if (schedule->timer != NULL) {
            esp_schedule_stop_timer(schedule);
        }
        schedule->trigger.next_scheduled_time_utc = 0;
    }
}

static void esp_schedule_start_timer(esp_schedule_t *schedule)
{
    time_t current_time = 0;
    ESP_SCHEDULE_GET_TIME(&current_time);
    if (current_time < SECONDS_TILL_2020) {
        ESP_SCHEDULE_LOGE(TAG, "Time is not updated");
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
        ESP_SCHEDULE_LOGW(TAG, "Schedule %s calculation failed or returned invalid time. Skipping timer creation.", schedule->name);
        /* Stop any already-armed timer so a stale diff cannot still fire, then
         * reset timestamp to indicate schedule is not active */
        if (schedule->timer) {
            esp_schedule_stop_timer(schedule);
        }
        schedule->trigger.next_scheduled_time_utc = 0;
        return;
    }

    ESP_SCHEDULE_LOGI(TAG, "Starting a timer for %"PRIu32" seconds for schedule %s", schedule->next_scheduled_time_diff, schedule->name);

    if (schedule->timestamp_cb) {
        schedule->timestamp_cb((esp_schedule_handle_t)schedule, (uint32_t)schedule->trigger.next_scheduled_time_utc, schedule->priv_data);
    }

    esp_schedule_arm_timer(schedule, schedule->next_scheduled_time_diff);
}

/* Self-deletion from a trigger callback ***************************************
 *
 * esp_schedule_delete() called from inside trigger_cb would free the schedule
 * that esp_schedule_common_timer_cb is still holding on its stack and re-arms
 * below -> use-after-free. A one-shot schedule that deletes itself when it fires
 * is a natural pattern, so the free is deferred instead: delete() tears down the
 * timer and the NVS entry as usual but leaves the allocation to the callback,
 * which returns without re-arming.
 *
 * The two flags below need no lock, but they do rely on a port property: the
 * timer implementation must serialize callback bodies against each other, so at
 * most one dispatch is in flight process-wide. esp_schedule_timer_ops_t requires
 * this explicitly - see the note on the ops table in esp_schedule.h. Given
 * that, and because delete() only consults the flags AFTER
 * esp_schedule_delete_timer() has barriered against any callback running on
 * another task, reaching that point with s_dispatching still equal to this
 * schedule means the caller IS its running callback.
 *
 * A port that dispatched two schedules concurrently on two tasks would break
 * this: the second dispatch overwrites s_dispatching, so the first schedule's
 * self-delete would not be deferred and would free an allocation its own
 * callback is still using. Making the state per-schedule instead of global would
 * remove the requirement, but esp_schedule_t is persisted to NVS verbatim and
 * esp_schedule_nvs_read_one() rejects any blob whose size does not match, so
 * growing the struct would discard every schedule stored by an earlier build.
 * The contract is the cheaper half of that trade for now. */
static esp_schedule_t *s_dispatching = NULL;
static bool s_dispatch_deleted = false;

static void esp_schedule_common_timer_cb(void *priv_data)
{
    esp_schedule_t *schedule = (esp_schedule_t *)priv_data;

    /* Guard against a premature timer expiry: if the scheduled instant has not
     * actually arrived (e.g. tick truncation on a very long delay that had to be
     * clamped), re-arm for the remaining time instead of firing early.
     * esp_schedule_start_timer recomputes the diff from the still-future
     * next_scheduled_time_utc, so it re-arms for what remains. */
    time_t now = 0;
    ESP_SCHEDULE_GET_TIME(&now);
    if (schedule->trigger.next_scheduled_time_utc > now) {
        ESP_SCHEDULE_LOGW(TAG, "Schedule %s fired early; rescheduling for the remaining time", schedule->name);
        esp_schedule_start_timer(schedule);
        return;
    }

    /* Re-check the validity window at fire time. The occurrence was computed to
     * be within [start,end], but callback dispatch can be delayed past end_time
     * (tick truncation, timer-queue latency, system load). Suppress a trigger
     * that is now outside the window; the re-arm below will find no further
     * valid occurrence and leave the schedule disarmed. */
    if (schedule->validity.end_time != 0 && now > schedule->validity.end_time) {
        ESP_SCHEDULE_LOGW(TAG, "Schedule %s expired before dispatch; suppressing out-of-window trigger", schedule->name);
        esp_schedule_start_timer(schedule);
        return;
    }

    ESP_SCHEDULE_LOGI(TAG, "Schedule %s triggered", schedule->name);
    if (schedule->trigger_cb) {
        s_dispatching = schedule;
        s_dispatch_deleted = false;
        schedule->trigger_cb((esp_schedule_handle_t)schedule, schedule->priv_data);
        bool deleted = s_dispatch_deleted;
        s_dispatching = NULL;
        s_dispatch_deleted = false;
        if (deleted) {
            /* The callback deleted this schedule; complete the deferred free and
             * do not touch it again. */
            ESP_SCHEDULE_FREE(schedule);
            return;
        }
    }

    esp_schedule_start_timer(schedule);
}

static void esp_schedule_delete_timer(esp_schedule_t *schedule)
{
    g_esp_schedule_port.timer.cancel(&schedule->timer);
}

static void esp_schedule_prepare_relative_target(esp_schedule_t *schedule)
{
    /* RELATIVE only: computing the diff here anchors the absolute target at
     * create time (the target is computed once and then reused, see
     * esp_schedule_set_next_scheduled_time_utc), and makes it readable via
     * esp_schedule_get() before the schedule is enabled. Every other type is
     * recomputed from scratch in esp_schedule_start_timer(), so precomputing
     * here would only duplicate work. NVS-enabled implies time is already
     * synced, so the calculation is valid. */
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE && esp_schedule_nvs_is_enabled()) {
        schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff(schedule);
    }
}

esp_err_t esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
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
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_start_timer(schedule);
    /* Persist the armed target of a one-shot schedule: create/edit store it while
     * it is still 0, which is indistinguishable from "never armed", so a reboot
     * after it fired would recompute and re-arm it (see
     * esp_schedule_trigger_fired_and_done). Repeating triggers never consult the
     * stored value, so they are not written. */
    if (schedule->trigger.next_scheduled_time_utc > 0 && esp_schedule_trigger_is_one_shot(&schedule->trigger)) {
        esp_schedule_nvs_add(schedule);
    }
    return ESP_OK;
}

esp_err_t esp_schedule_disable(esp_schedule_handle_t handle)
{
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
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

/*
 * Validate the time-of-day fields. RELATIVE schedules do not use hours/minutes.
 * An out-of-range value would otherwise be silently normalized by mktime() into
 * a following day, landing on a day that violates the configured day/month mask.
 */
static bool esp_schedule_config_time_of_day_is_valid(const esp_schedule_config_t *schedule_config)
{
    if (schedule_config->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        return true;
    }
    return (schedule_config->trigger.hours < 24 && schedule_config->trigger.minutes < 60);
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
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    if (handle == NULL || schedule_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    if (strncmp(schedule->name, schedule_config->name, sizeof(schedule->name)) != 0) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule name mismatch. Expected: %s, Passed: %s", schedule->name, schedule_config->name);
        return ESP_FAIL;
    }

    if (!esp_schedule_config_time_of_day_is_valid(schedule_config)) {
        ESP_SCHEDULE_LOGE(TAG, "Invalid time of day for schedule %s: %u:%u. Expected hours in [0,23] and minutes in [0,59].",
                          schedule_config->name, (unsigned int)schedule_config->trigger.hours, (unsigned int)schedule_config->trigger.minutes);
        return ESP_ERR_INVALID_ARG;
    }

    if (!esp_schedule_trigger_is_valid(&schedule_config->trigger, schedule_config->name)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Editing a schedule with relative time should also reset it. */
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule->trigger.next_scheduled_time_utc = 0;
    }
    esp_schedule_set(schedule, schedule_config);
    ESP_SCHEDULE_LOGD(TAG, "Schedule %s edited", schedule->name);
    return ESP_OK;
}

esp_err_t esp_schedule_delete(esp_schedule_handle_t handle)
{
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    ESP_SCHEDULE_LOGI(TAG, "Deleting schedule %s", schedule->name);
    if (schedule->timer) {
        esp_schedule_stop_timer(schedule);
        esp_schedule_delete_timer(schedule);
    }
    esp_schedule_nvs_remove(schedule);
    /* Checked only after the timer teardown above, which barriers against a
     * callback dispatching this schedule on another task. Still being the
     * dispatched schedule here means this is a self-delete from its own
     * trigger_cb, whose stack frame outlives us; hand the free back to it. */
    if (schedule == s_dispatching) {
        s_dispatch_deleted = true;
        return ESP_OK;
    }
    ESP_SCHEDULE_FREE(schedule);
    return ESP_OK;
}

esp_schedule_handle_t esp_schedule_create(esp_schedule_config_t *schedule_config)
{
    if (!esp_schedule_is_inited()) {
        ESP_SCHEDULE_LOGE(TAG, "esp_schedule_init() must be called first");
        return NULL;
    }
    if (schedule_config == NULL) {
        return NULL;
    }
    if (strlen(schedule_config->name) <= 0) {
        ESP_SCHEDULE_LOGE(TAG, "Set schedule failed. Please enter a unique valid name for the schedule.");
        return NULL;
    }

    if (schedule_config->trigger.type == ESP_SCHEDULE_TYPE_INVALID) {
        ESP_SCHEDULE_LOGE(TAG, "Schedule type is invalid.");
        return NULL;
    }

    if (!esp_schedule_config_time_of_day_is_valid(schedule_config)) {
        ESP_SCHEDULE_LOGE(TAG, "Invalid time of day for schedule %s: %u:%u. Expected hours in [0,23] and minutes in [0,59].",
                          schedule_config->name, (unsigned int)schedule_config->trigger.hours, (unsigned int)schedule_config->trigger.minutes);
        return NULL;
    }

    if (!esp_schedule_trigger_is_valid(&schedule_config->trigger, schedule_config->name)) {
        return NULL;
    }

    esp_schedule_t *schedule = (esp_schedule_t *)ESP_SCHEDULE_CALLOC(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate handle");
        return NULL;
    }
    strlcpy(schedule->name, schedule_config->name, sizeof(schedule->name));

    esp_schedule_set(schedule, schedule_config);

    esp_schedule_prepare_relative_target(schedule);
    ESP_SCHEDULE_LOGD(TAG, "Schedule %s created", schedule->name);
    return (esp_schedule_handle_t)schedule;
}

esp_schedule_handle_t *esp_schedule_init_with_config(const esp_schedule_port_config_t *port,
        bool enable_nvs, char *nvs_partition,
        uint8_t *schedule_count)
{
    /* Clear the out-parameter up front so every early return below reports zero
     * restored schedules rather than leaving the caller's variable untouched.
     * Doing it here rather than per-return is what keeps them all consistent. */
    if (schedule_count != NULL) {
        *schedule_count = 0;
    }

    esp_err_t err = esp_schedule_port_install(port);
    if (err != ESP_OK) {
        /* Nothing is usable without a port, so there is no partial success to
         * report here: the caller gets no schedules and every later API call
         * will refuse with ESP_ERR_INVALID_STATE. */
        return NULL;
    }

    if (g_esp_schedule_port.time_sync.timesync_init != NULL) {
        g_esp_schedule_port.time_sync.timesync_init();
    }

    if (!enable_nvs) {
        return NULL;
    }

    if (schedule_count == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "schedule_count cannot be NULL when NVS is enabled");
        return NULL;
    }

    /* Wait for time to be updated here */

    /* Below this is initialising schedules from NVS */
    if (esp_schedule_nvs_init(nvs_partition) != ESP_OK) {
        /* Either the port has no storage or the partition name could not be
         * copied. Schedules still work, they just will not persist. */
        return NULL;
    }

    /* Get handle list from NVS */
    esp_schedule_handle_t *handle_list = esp_schedule_nvs_get_all(schedule_count);
    if (handle_list == NULL) {
        ESP_SCHEDULE_LOGI(TAG, "No schedules found in NVS");
        return NULL;
    }
    ESP_SCHEDULE_LOGI(TAG, "Schedules found in NVS: %"PRIu8, *schedule_count);
    /* Start/Delete the schedules */
    esp_schedule_t *schedule = NULL;
    for (size_t handle_count = 0; handle_count < *schedule_count; handle_count++) {
        schedule = (esp_schedule_t *)handle_list[handle_count];
        schedule->trigger_cb = NULL;
        schedule->timestamp_cb = NULL;
        schedule->timer = NULL;
        /* Drop schedules we cannot arm: a stored config may have been written by
         * an older version that accepted combinations now rejected, and a
         * schedule with no valid future occurrence (already-fired one-shot, or a
         * year bounded in the past) is expired. */
        bool has_future = esp_schedule_trigger_is_valid(&schedule->trigger, schedule->name) &&
                          esp_schedule_set_next_scheduled_time_utc(schedule->name, &schedule->trigger, &schedule->validity);
        if (!has_future) {
            /* This schedule is invalid or has already expired. */
            ESP_SCHEDULE_LOGI(TAG, "Schedule %s cannot be armed (invalid config, or does not repeat and has already expired). Deleting it.", schedule->name);
            esp_schedule_delete((esp_schedule_handle_t)schedule);
            /* Removing the schedule from the list */
            handle_list[handle_count] = handle_list[*schedule_count - 1];
            (*schedule_count)--;
            handle_count--;
            continue;
        }
        esp_schedule_prepare_relative_target(schedule);
        esp_schedule_start_timer(schedule);
    }
    return handle_list;
}
