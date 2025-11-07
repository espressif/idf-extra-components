/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_schedule_internal.h"
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
#include "esp_daylight.h"
#endif

static const char *TAG = "test_app";

static void print_time(const char *label, time_t t)
{
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z[%Z]", &tm_local);
    ESP_LOGI(TAG, "%s: %s (%ld)", label, buf, (long)t);
}

static time_t make_time_utc(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm tmv = {0};
    tmv.tm_year = year - 1900;
    tmv.tm_mon = mon - 1;
    tmv.tm_mday = mday;
    tmv.tm_hour = hour;
    tmv.tm_min = min;
    tmv.tm_sec = sec;
    return mktime(&tmv);
}

static void assert_time_eq(const char *name, time_t got, time_t want)
{
    if (got != want) {
        print_time("got ", got);
        print_time("want", want);
    }
    TEST_ASSERT_TRUE_MESSAGE(got == want, name);
}

// --- Date permutations ---
TEST_CASE("date permutations", "[esp_schedule]")
{
    time_t now = make_time_utc(2025, 1, 16, 12, 0, 0); // Thu
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 365 * 24 * 3600 };

    // 17th at 00:24
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, /*00:24*/24, /*days_of_week*/0, /*day_of_month*/17, /*months*/0, /*year*/0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: 17th 00:24");
    assert_time_eq("date: 17th 00:24", next_ts, make_time_utc(2025, 1, 17, 0, 24, 0));

    // Specific month mask (Jan, Mar) on 20th at 08:00 => Jan 20 since we're in Jan
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 8 * 60, 0, 20, (1u << 0) | (1u << 2), 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: month mask Jan/Mar day=20 08:00");
    assert_time_eq("date: month mask Jan/Mar day=20 08:00", next_ts, make_time_utc(2025, 1, 20, 8, 0, 0));

    // Specific year constraint (2026) day 5 at 09:15
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 9 * 60 + 15, 0, 5, 0, 2026, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: year=2026 day=5 09:15");
    assert_time_eq("date: year=2026 day=5 09:15", next_ts, make_time_utc(2026, 1, 5, 9, 15, 0));
}

// --- More date permutations ---
TEST_CASE("date permutations more", "[esp_schedule]")
{
    // Day=31 with months mask including a 30-day month (Apr) and 31-day month (May)
    time_t now = make_time_utc(2025, 4, 29, 10, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 400 * 24 * 3600 };

    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 6 * 60, 0, 31, (1u << 3) | (1u << 4), 0, &validity, &next_ts); // Apr(3), May(4)
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: 31st across months");
    assert_time_eq("date: 31st across months -> May 31 06:00", next_ts, make_time_utc(2025, 5, 31, 6, 0, 0));

    // Month rollover year: months {Nov, Dec, Jan}, day=1 at 00:00 from Dec 31
    now = make_time_utc(2025, 12, 31, 23, 30, 0);
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 0, 0, 1, (1u << 10) | (1u << 11) | (1u << 0), 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: Nov/Dec/Jan day=1 at year boundary");
    assert_time_eq("date: Nov/Dec/Jan day=1 -> Jan 1 00:00", next_ts, make_time_utc(2026, 1, 1, 0, 0, 0));
}

// --- Day of week ---
TEST_CASE("day of week", "[esp_schedule]")
{
    time_t now = make_time_utc(2025, 1, 16, 7, 45, 0); // Thu 07:45
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 30 * 24 * 3600 };

    uint8_t days_of_week = (1 << 0) | (1 << 1); // Mon/Tue
    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 8 * 60 + 30, days_of_week, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "dow: Mon/Tue 08:30");
    assert_time_eq("dow: Mon/Tue 08:30", next_ts, make_time_utc(2025, 1, 20, 8, 30, 0));
}

// --- Hybrid OR (weekday OR date) ---
TEST_CASE("hybrid dow or date", "[esp_schedule]")
{
    time_t now = make_time_utc(2025, 1, 16, 7, 45, 0); // Thu 07:45
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 40 * 24 * 3600 };

    uint8_t days_of_week = (1 << 0) | (1 << 1);
    time_t a = 0, b = 0; bool ok_a, ok_b;
    ok_a = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &a);
    ok_b = esp_schedule_get_next_date_time(now, 30, 0, 17, 0, 0, &validity, &b);
    TEST_ASSERT_TRUE_MESSAGE(ok_a && ok_b, "hybrid: Mon/Tue 09:00 OR 17th 00:30");

    time_t chosen = (a < b) ? a : b;
    assert_time_eq("hybrid: Mon/Tue 09:00 OR 17th 00:30", chosen, make_time_utc(2025, 1, 17, 0, 30, 0));
}

// --- Knife edge: now equals target -> should select next occurrence ---
TEST_CASE("knife edge now equals target", "[esp_schedule]")
{
    time_t now = make_time_utc(2025, 1, 16, 8, 0, 0); // Thu 08:00
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10 * 24 * 3600 };

    uint8_t days_of_week = (1 << 3); // Thursday
    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 8 * 60, days_of_week, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "knife-edge: now != target (Thu 08:00)");

    // should be the next Thursday at 08:00
    assert_time_eq("knife-edge: now != target (Thu 08:00)", next_ts, make_time_utc(2025, 1, 23, 8, 0, 0));
}

// --- Validity window ---
TEST_CASE("validity respected", "[esp_schedule]")
{
    time_t now = make_time_utc(2025, 1, 16, 23, 50, 0);
    esp_schedule_validity_t validity = { .start_time = now + 20 * 60, .end_time = now + 2 * 24 * 3600 };

    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: start boundary honored");
    assert_time_eq("validity: start boundary honored", next_ts, validity.start_time);
}

// --- Sequences for same trigger type ---
TEST_CASE("sequence dow mon wed", "[esp_schedule]")
{
    // Sequence Mon/Wed 09:00 from Monday 08:50 -> Mon 09:00, Wed 09:00, next Mon 09:00
    time_t now = make_time_utc(2025, 1, 13, 8, 50, 0); // Monday
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 30 * 24 * 3600 };
    uint8_t days_of_week = (1 << 0) | (1 << 2); // Mon, Wed

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: first");
    assert_time_eq("seq dow: first", t1, make_time_utc(2025, 1, 13, 9, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 9 * 60, days_of_week, 0, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: second");
    assert_time_eq("seq dow: second", t2, make_time_utc(2025, 1, 15, 9, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 9 * 60, days_of_week, 0, 0, 0, &validity, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: third");
    assert_time_eq("seq dow: third", t3, make_time_utc(2025, 1, 20, 9, 0, 0));
}

TEST_CASE("sequence date months mask", "[esp_schedule]")
{
    // Day=15 at 07:00 for months {Jan, Mar, Apr}
    time_t now = make_time_utc(2025, 1, 10, 7, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 370 * 24 * 3600 };
    uint16_t months = (1u << 0) | (1u << 2) | (1u << 3); // Jan, Mar, Apr

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, months, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: first");
    assert_time_eq("seq date: first", t1, make_time_utc(2025, 1, 15, 7, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 7 * 60, 0, 15, months, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: second");
    assert_time_eq("seq date: second", t2, make_time_utc(2025, 3, 15, 7, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 7 * 60, 0, 15, months, 0, &validity, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: third");
    assert_time_eq("seq date: third", t3, make_time_utc(2025, 4, 15, 7, 0, 0));
}

TEST_CASE("sequence validity cutoff", "[esp_schedule]")
{
    // Validity end should stop sequences
    time_t now = make_time_utc(2025, 1, 13, 8, 50, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_utc(2025, 1, 16, 0, 0, 0) };
    uint8_t days_of_week = (1 << 0) | (1 << 2); // Mon, Wed

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq cutoff: first");
    assert_time_eq("seq cutoff: first", t1, make_time_utc(2025, 1, 13, 9, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 9 * 60, days_of_week, 0, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq cutoff: second");
    assert_time_eq("seq cutoff: second", t2, make_time_utc(2025, 1, 15, 9, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 9 * 60, days_of_week, 0, 0, 0, &validity, &t3);
    TEST_ASSERT_FALSE_MESSAGE(ok, "seq cutoff: third should fail due to validity end");
    TEST_ASSERT_TRUE(t3 == 0);
}

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
TEST_CASE("solar with dow", "[esp_schedule]")
{
    double lat = 37.7749, lon = -122.4194; // San Francisco, CA
    time_t now = make_time_utc(2025, 1, 12, 6, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 15 * 24 * 3600 };

    esp_schedule_trigger_t tr = (esp_schedule_trigger_t) {
        0
    };
    tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_TUESDAY | ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_THURSDAY | ESP_SCHEDULE_DAY_FRIDAY;
    tr.solar.latitude = lat; tr.solar.longitude = lon; tr.solar.offset_minutes = 0;

    /* Get first expected sunrise of the triggered day */
    time_t last_solar = now;
    for (int day = 13; day <= 17; day++) {
        time_t sunrise = 0, sunset = 0;
        bool ok = esp_daylight_calc_sunrise_sunset_utc(2025, 1, day, lat, lon, &sunrise, &sunset);
        TEST_ASSERT_TRUE_MESSAGE(ok, "sunrise/sunset calculation");
        TEST_ASSERT_NOT_EQUAL(0, sunrise);
        TEST_ASSERT_NOT_EQUAL(0, sunset);
        last_solar = esp_schedule_get_next_valid_solar_time(last_solar, &tr, &validity, "solar_dow");
        char buf[128];
        snprintf(buf, sizeof(buf), "solar: day %d: failed to get next valid solar time", day);
        TEST_ASSERT_TRUE_MESSAGE(last_solar != 0, buf);
        snprintf(buf, sizeof(buf), "solar: day %d: %" PRIu32 " != %" PRIu32, day, (uint32_t)last_solar, (uint32_t)sunrise);
        TEST_ASSERT_TRUE_MESSAGE(last_solar == sunrise, buf);
    }
}

TEST_CASE("solar with date mask", "[esp_schedule]")
{
    double lat = 52.5200, lon = 13.4050; // Berlin, Germany
    // Use midday to avoid edge near-sunset timing
    time_t now = make_time_utc(2025, 6, 15, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 90 * 24 * 3600 };

    esp_schedule_trigger_t tr = (esp_schedule_trigger_t) {
        0
    };
    tr.type = ESP_SCHEDULE_TYPE_SUNSET;
    tr.date.day = 15;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY | ESP_SCHEDULE_MONTH_AUGUST;
    tr.solar.latitude = lat; tr.solar.longitude = lon; tr.solar.offset_minutes = -15;

    time_t last_solar = now;
    for (int month = 6; month <= 8; month++) {
        time_t sunrise = 0, sunset = 0;
        bool ok = esp_daylight_calc_sunrise_sunset_utc(2025, month, 15, lat, lon, &sunrise, &sunset);
        TEST_ASSERT_TRUE_MESSAGE(ok, "sunrise/sunset calculation");
        TEST_ASSERT_NOT_EQUAL(0, sunset);
        last_solar = esp_schedule_get_next_valid_solar_time(last_solar, &tr, &validity, "solar_date_mask");
        char buf[128];
        snprintf(buf, sizeof(buf), "solar: month %d: failed to get next valid solar time", month);
        TEST_ASSERT_TRUE_MESSAGE(last_solar != 0, buf);
        time_t expected = sunset - 15 * 60;
        snprintf(buf, sizeof(buf), "solar: month %d: %" PRIu32 " != %" PRIu32, month, (uint32_t)last_solar, (uint32_t)expected);
        TEST_ASSERT_TRUE_MESSAGE(last_solar == expected, buf);
    }
}

TEST_CASE("solar sequence monotonic", "[esp_schedule]")
{
    int year = 2025, month = 1, day = 12; // Jan 12, 2025: Sunday
    double lat = 37.7749, lon = -122.4194; // San Francisco, CA
    time_t now = make_time_utc(year, month, day, 0, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10 * 24 * 3600 };

    esp_schedule_trigger_t tr = (esp_schedule_trigger_t) {
        0
    };
    tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_FRIDAY; // Mon, Wed, Fri
    tr.solar.latitude = lat; tr.solar.longitude = lon; tr.solar.offset_minutes = 0;

    /* Get first expected sunrise of the triggered day */
    time_t sunrise = 0, sunset = 0;
    bool ok = esp_daylight_calc_sunrise_sunset_utc(year, month, day + 1, /*next day*/ lat, lon, &sunrise, &sunset);
    TEST_ASSERT_TRUE_MESSAGE(ok, "sunrise/sunset calculation");
    TEST_ASSERT_NOT_EQUAL(0, sunrise);
    TEST_ASSERT_NOT_EQUAL(0, sunset);

    time_t s1 = esp_schedule_get_next_valid_solar_time(now, &tr, &validity, "solar_seq");
    TEST_ASSERT_TRUE_MESSAGE(s1 != 0, "solar seq first");

    char buf[128];
    snprintf(buf, sizeof(buf), "solar seq first: %" PRIu32 " != %" PRIu32, (uint32_t)s1, (uint32_t)sunrise);
    TEST_ASSERT_TRUE_MESSAGE(s1 == sunrise, buf);

    /* Get next expected sunrise on next triggered day */
    ok = esp_daylight_calc_sunrise_sunset_utc(year, month, day + 3, /*next triggered day*/ lat, lon, &sunrise, &sunset);
    TEST_ASSERT_TRUE_MESSAGE(ok, "sunrise/sunset calculation");
    TEST_ASSERT_NOT_EQUAL(0, sunrise);
    TEST_ASSERT_NOT_EQUAL(0, sunset);

    time_t s2 = esp_schedule_get_next_valid_solar_time(s1, &tr, &validity, "solar_seq");
    TEST_ASSERT_TRUE_MESSAGE(s2 != 0, "solar seq second");
    snprintf(buf, sizeof(buf), "solar seq second: %" PRIu32 " != %" PRIu32, (uint32_t)s2, (uint32_t)sunrise);
    TEST_ASSERT_TRUE_MESSAGE(s2 == sunrise, buf);
    TEST_ASSERT_TRUE_MESSAGE(s2 > s1, "solar seq monotonic");
}
#endif

// --- NVS Tests ---

static void __match_trigger(esp_schedule_trigger_t *got, esp_schedule_trigger_t *want)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->type, want->type, "Trigger types should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->hours, want->hours, "Trigger hours should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->minutes, want->minutes, "Trigger minutes should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->day.repeat_days, want->day.repeat_days, "Trigger days should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->date.day, want->date.day, "Trigger date should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(got->date.repeat_months, want->date.repeat_months, "Trigger months should match");
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(got->solar.latitude, want->solar.latitude, "Trigger latitude should match");
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(got->solar.longitude, want->solar.longitude, "Trigger longitude should match");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(got->solar.offset_minutes, want->solar.offset_minutes, "Trigger offset minutes should match");
}

TEST_CASE("nvs basic operations", "[esp_schedule]")
{
    // Create a simple schedule config
    esp_schedule_config_t config = {0};
    strcpy(config.name, "test_schedule");
    config.triggers.count = 1;
    config.triggers.list = (esp_schedule_trigger_t *)malloc(sizeof(esp_schedule_trigger_t));
    config.triggers.list[0].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    config.triggers.list[0].hours = 8;
    config.triggers.list[0].minutes = 0;
    config.triggers.list[0].day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    config.validity.start_time = 0;
    config.validity.end_time = 2147483647; // Max time_t

    // Create schedule
    esp_schedule_handle_t handle = NULL;
    esp_err_t err = esp_schedule_create(&config, &handle);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, (int)err, "Failed to create schedule");
    TEST_ASSERT_NOT_NULL_MESSAGE(handle, "Failed to create schedule");

    // Retrieve all schedules from NVS
    uint8_t count = 0;
    esp_schedule_handle_t *handles = esp_schedule_nvs_get_all(&count);
    ESP_LOGI(TAG, "Schedules in NVS: %d", count);
    TEST_ASSERT_NOT_NULL_MESSAGE(handles, "Failed to get schedules from NVS");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, count, "Should have 1 schedule in NVS");

    // Find our test schedule in the retrieved list
    esp_schedule_handle_t retrieved_handle = NULL;
    for (int i = 0; i < count; i++) {
        esp_schedule_config_t retrieved_config = {0};
        esp_schedule_get(handles[i], &retrieved_config);
        if (strcmp(retrieved_config.name, config.name) == 0) {
            retrieved_handle = handles[i];
            esp_schedule_config_free_internals(&retrieved_config);
            break;
        }
        esp_schedule_config_free_internals(&retrieved_config);
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(retrieved_handle, "Test schedule not found in retrieved list");

    // Verify retrieved schedule has same data
    esp_schedule_config_t retrieved_config = {0};
    esp_schedule_get(retrieved_handle, &retrieved_config);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(config.name, retrieved_config.name, "Schedule names should match");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(config.triggers.count, retrieved_config.triggers.count, "Trigger counts should match");
    __match_trigger(&config.triggers.list[0], &retrieved_config.triggers.list[0]);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(config.validity.start_time, retrieved_config.validity.start_time, "Validity start time should match");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(config.validity.end_time, retrieved_config.validity.end_time, "Validity end time should match");

    // Clean up retrieved config
    esp_schedule_config_free_internals(&retrieved_config);

    // Clean up
    esp_schedule_delete(handle);
    for (int i = 0; i < count; i++) {
        esp_schedule_delete(handles[i]);
    }
    free(handles);
    free(config.triggers.list);

    // Verify removal - should be 0 schedules
    handles = esp_schedule_nvs_get_all(&count);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, count, "Should have 0 schedules after removal");
    if (handles) {
        free(handles);
    }
}

TEST_CASE("nvs multiple schedules", "[esp_schedule]")
{
    // Create multiple schedules
    esp_schedule_config_t configs[3] = {0};
    const char *names[3] = {"schedule1", "schedule2", "schedule3"};

    for (int i = 0; i < 3; i++) {
        strcpy(configs[i].name, names[i]);
        configs[i].triggers.count = 1;
        configs[i].triggers.list = (esp_schedule_trigger_t *)malloc(sizeof(esp_schedule_trigger_t));
        configs[i].triggers.list[0].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
        configs[i].triggers.list[0].hours = 8 + i;
        configs[i].triggers.list[0].minutes = i * 15;
        configs[i].triggers.list[0].day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
        configs[i].validity.start_time = 0;
        configs[i].validity.end_time = 2147483647;

        esp_schedule_handle_t handle = NULL;
        esp_err_t err = esp_schedule_create(&configs[i], &handle);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, (int)err, "Failed to create schedule");
        TEST_ASSERT_NOT_NULL_MESSAGE(handle, "Failed to create schedule");
    }

    // Get all schedules
    uint8_t retrieved_count = 0;
    esp_schedule_handle_t *handles = esp_schedule_nvs_get_all(&retrieved_count);
    TEST_ASSERT_NOT_NULL_MESSAGE(handles, "Failed to get all schedules");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, retrieved_count, "Should retrieve 3 schedules");

    // Find and verify each expected schedule by name
    bool found_schedules[3] = {false, false, false};
    for (int i = 0; i < retrieved_count; i++) {
        esp_schedule_config_t retrieved_config = {0};
        esp_schedule_get(handles[i], &retrieved_config);

        // Check which schedule this is
        for (int j = 0; j < 3; j++) {
            if (strcmp(retrieved_config.name, names[j]) == 0) {
                TEST_ASSERT_FALSE_MESSAGE(found_schedules[j], "Duplicate schedule found");
                found_schedules[j] = true;
                __match_trigger(&configs[j].triggers.list[0], &retrieved_config.triggers.list[0]);
                free(configs[j].triggers.list);
                break;
            }
        }
        esp_schedule_config_free_internals(&retrieved_config);
        esp_schedule_delete(handles[i]);
    }
    free(handles);

    // Verify all expected schedules were found
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE_MESSAGE(found_schedules[i], "Expected schedule not found");
    }

    // Verify all removed
    handles = esp_schedule_nvs_get_all(&retrieved_count);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, retrieved_count, "Should have 0 schedules after remove_all");
    if (handles) {
        free(handles);
    }
}

TEST_CASE("nvs schedule with multiple triggers", "[esp_schedule]")
{
    // Create schedule with multiple triggers
    esp_schedule_config_t config = {0};
    strcpy(config.name, "multi_trigger");
    config.triggers.count = 3;
    config.triggers.list = (esp_schedule_trigger_t *)malloc(3 * sizeof(esp_schedule_trigger_t));

    // First trigger: Monday 8:00
    config.triggers.list[0].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    config.triggers.list[0].hours = 8;
    config.triggers.list[0].minutes = 0;
    config.triggers.list[0].day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;

    // Second trigger: Wednesday 14:30
    config.triggers.list[1].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    config.triggers.list[1].hours = 14;
    config.triggers.list[1].minutes = 30;
    config.triggers.list[1].day.repeat_days = ESP_SCHEDULE_DAY_WEDNESDAY;

    // Third trigger: Friday 18:45
    config.triggers.list[2].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    config.triggers.list[2].hours = 18;
    config.triggers.list[2].minutes = 45;
    config.triggers.list[2].day.repeat_days = ESP_SCHEDULE_DAY_FRIDAY;

    config.validity.start_time = 0;
    config.validity.end_time = 2147483647;

    // Create and store schedule
    esp_schedule_handle_t handle = NULL;
    esp_err_t err = esp_schedule_create(&config, &handle);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, (int)err, "Failed to create schedule");
    TEST_ASSERT_NOT_NULL_MESSAGE(handle, "Failed to create schedule");

    // Retrieve all schedules and find our test schedule
    uint8_t count = 0;
    esp_schedule_handle_t *handles = esp_schedule_nvs_get_all(&count);
    TEST_ASSERT_NOT_NULL_MESSAGE(handles, "Failed to get schedules from NVS");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, count, "Should have 1 schedule in NVS");

    esp_schedule_handle_t retrieved_handle = NULL;
    for (int i = 0; i < count; i++) {
        esp_schedule_config_t retrieved_config = {0};
        esp_schedule_get(handles[i], &retrieved_config);
        if (strcmp(retrieved_config.name, config.name) == 0) {
            retrieved_handle = handles[i];
        } else {
            esp_schedule_delete(handles[i]); // Clean up non-matching schedules immediately
        }
        esp_schedule_config_free_internals(&retrieved_config);
    }

    TEST_ASSERT_NOT_NULL_MESSAGE(retrieved_handle, "Test schedule not found in retrieved list");

    esp_schedule_config_t retrieved_config = {0};
    esp_schedule_get(retrieved_handle, &retrieved_config);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, retrieved_config.triggers.count, "Should have 3 triggers");
    for (int i = 0; i < 3; i++) {
        __match_trigger(&config.triggers.list[i], &retrieved_config.triggers.list[i]);
    }

    // Clean up retrieved config
    esp_schedule_config_free_internals(&retrieved_config);

    // Clean up
    esp_schedule_delete(handle);
    esp_schedule_delete(retrieved_handle);
    free(handles);
    free(config.triggers.list);
}

TEST_CASE("nvs delete all", "[esp_schedule]")
{
    // Create multiple schedules for bulk deletion test
    esp_schedule_config_t configs[3] = {0};
    esp_schedule_handle_t handles[3] = {NULL, NULL, NULL};
    const char *names[3] = {"delete_test1", "delete_test2", "delete_test3"};

    for (int i = 0; i < 3; i++) {
        strcpy(configs[i].name, names[i]);
        configs[i].triggers.count = 1;
        configs[i].triggers.list = (esp_schedule_trigger_t *)malloc(sizeof(esp_schedule_trigger_t));
        configs[i].triggers.list[0].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
        configs[i].triggers.list[0].hours = 9 + i;
        configs[i].triggers.list[0].minutes = i * 10;
        configs[i].triggers.list[0].day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
        configs[i].validity.start_time = 0;
        configs[i].validity.end_time = 2147483647;

        esp_err_t err = esp_schedule_create(&configs[i], &handles[i]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, (int)err, "Failed to create schedule");
        TEST_ASSERT_NOT_NULL_MESSAGE(handles[i], "Failed to create schedule");
    }

    // Verify schedules were added to NVS
    uint8_t count_before = 0;
    esp_schedule_handle_t *all_handles_before = esp_schedule_nvs_get_all(&count_before);
    TEST_ASSERT_NOT_NULL_MESSAGE(all_handles_before, "Failed to get schedules from NVS");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, count_before, "Should have 3 schedules before delete_all");

    // Test esp_schedule_delete_all with our handles
    ESP_SCHEDULE_RETURN_TYPE result = esp_schedule_delete_all(handles, 3);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_SCHEDULE_RET_OK, result, "esp_schedule_delete_all should succeed");

    // Verify schedules were deleted from NVS
    uint8_t count_after = 0;
    esp_schedule_handle_t *all_handles_after = esp_schedule_nvs_get_all(&count_after);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, count_after, "Should have 0 schedules after delete_all");

    if (all_handles_after) {
        free(all_handles_after);
    }
    free(all_handles_before);
    for (int i = 0; i < 3; i++) {
        free(configs[i].triggers.list);
    }
}
