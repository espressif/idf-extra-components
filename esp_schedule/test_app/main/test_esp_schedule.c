/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_netif.h"
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

/* Builds an epoch time_t from a broken-down LOCAL wall-clock time (mktime
 * interprets tm in the active TZ, not UTC). Tests build both `now` and expected
 * instants with this same helper, so comparisons stay TZ-independent; the DST
 * tests set TZ=NY on purpose to exercise wall-clock behavior. */
static time_t make_time_local(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm tmv = {0};
    tmv.tm_year = year - 1900;
    tmv.tm_mon = mon - 1;
    tmv.tm_mday = mday;
    tmv.tm_hour = hour;
    tmv.tm_min = min;
    tmv.tm_sec = sec;
    tmv.tm_isdst = -1; /* let mktime resolve DST for the active timezone */
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
    time_t now = make_time_local(2025, 1, 16, 12, 0, 0); // Thu
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 365 * 24 * 3600 };

    // 17th at 00:24
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, /*00:24*/24, /*days_of_week*/0, /*day_of_month*/17, /*months*/0, /*year*/0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: 17th 00:24");
    assert_time_eq("date: 17th 00:24", next_ts, make_time_local(2025, 1, 17, 0, 24, 0));

    // Specific month mask (Jan, Mar) on 20th at 08:00 => Jan 20 since we're in Jan
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 8 * 60, 0, 20, (1u << 0) | (1u << 2), 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: month mask Jan/Mar day=20 08:00");
    assert_time_eq("date: month mask Jan/Mar day=20 08:00", next_ts, make_time_local(2025, 1, 20, 8, 0, 0));

    // Specific year constraint (2026) day 5 at 09:15
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 9 * 60 + 15, 0, 5, 0, 2026, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: year=2026 day=5 09:15");
    assert_time_eq("date: year=2026 day=5 09:15", next_ts, make_time_local(2026, 1, 5, 9, 15, 0));
}

// --- More date permutations (day-31 across months, year rollover) ---
TEST_CASE("date permutations more", "[esp_schedule]")
{
    // Day=31 with months mask including a 30-day month (Apr) and 31-day month (May).
    // Regression: a day-31 schedule must NOT fire on a normalized Mar/May-1 after a
    // short month; it must land on May 31.
    time_t now = make_time_local(2025, 4, 29, 10, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 400 * 24 * 3600 };

    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 6 * 60, 0, 31, (1u << 3) | (1u << 4), 0, &validity, &next_ts); // Apr(3), May(4)
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: 31st across months");
    assert_time_eq("date: 31st across months -> May 31 06:00", next_ts, make_time_local(2025, 5, 31, 6, 0, 0));

    // Month rollover year: months {Nov, Dec, Jan}, day=1 at 00:00 from Dec 31
    now = make_time_local(2025, 12, 31, 23, 30, 0);
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 0, 0, 1, (1u << 10) | (1u << 11) | (1u << 0), 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: Nov/Dec/Jan day=1 at year boundary");
    assert_time_eq("date: Nov/Dec/Jan day=1 -> Jan 1 00:00", next_ts, make_time_local(2026, 1, 1, 0, 0, 0));
}

// --- Feb 29 in non-leap years must skip to the next leap Feb 29 ---
TEST_CASE("date feb29 non leap", "[esp_schedule]")
{
    // From Mar 2025 (non-leap), a Feb-29 schedule must NOT fire on Mar 1 2026;
    // it must land on Feb 29 2028 (next leap year).
    time_t now = make_time_local(2025, 3, 1, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 1500L * 24 * 3600 };

    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 29, ESP_SCHEDULE_MONTH_FEBRUARY, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date: Feb 29 skips non-leap years");
    assert_time_eq("date: Feb 29 -> 2028-02-29 09:00", next_ts, make_time_local(2028, 2, 29, 9, 0, 0));
}

// --- Day of week ---
TEST_CASE("day of week", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 16, 7, 45, 0); // Thu 07:45
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 30 * 24 * 3600 };

    uint8_t days_of_week = (1 << 0) | (1 << 1); // Mon/Tue
    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 8 * 60 + 30, days_of_week, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "dow: Mon/Tue 08:30");
    assert_time_eq("dow: Mon/Tue 08:30", next_ts, make_time_local(2025, 1, 20, 8, 30, 0));
}

// --- Caller-side OR of two independent single-arm schedules ---
// NOTE: this exercises CALLER composition (min of two separate engine calls),
// NOT the engine's built-in dow|dom OR. The engine's own union is covered by
// "date type dow or dom".
TEST_CASE("caller or two single-arm schedules", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 16, 7, 45, 0); // Thu 07:45
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 40 * 24 * 3600 };

    uint8_t days_of_week = (1 << 0) | (1 << 1);
    time_t a = 0, b = 0; bool ok_a, ok_b;
    ok_a = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &a);
    ok_b = esp_schedule_get_next_date_time(now, 30, 0, 17, 0, 0, &validity, &b);
    TEST_ASSERT_TRUE_MESSAGE(ok_a && ok_b, "caller-or: Mon/Tue 09:00 OR 17th 00:30");

    time_t chosen = (a < b) ? a : b;
    assert_time_eq("caller-or: Mon/Tue 09:00 OR 17th 00:30", chosen, make_time_local(2025, 1, 17, 0, 30, 0));
}

// --- DATE trigger honors day-of-week (OR'd with day-of-month) ---
TEST_CASE("date type dow or dom", "[esp_schedule]")
{
    /* Thu Jan 16 2025 07:45. A DATE trigger passing both a DOW mask (Mon) and a
     * day-of-month (17th) must fire on the union: the nearer of next Mon or the
     * 17th. From Thu, the 17th (Fri) comes before Mon the 20th. */
    time_t now = make_time_local(2025, 1, 16, 7, 45, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 40 * 24 * 3600 };

    uint8_t dow = ESP_SCHEDULE_DAY_MONDAY;
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 9 * 60, dow, 17, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date-dow-or-dom: Mon OR 17th 09:00");
    assert_time_eq("date-dow-or-dom: 17th before next Mon", next_ts, make_time_local(2025, 1, 17, 9, 0, 0));

    /* Next occurrence after the 17th is Monday the 20th (the DOW arm). */
    time_t t2 = 0;
    ok = esp_schedule_get_next_date_time(next_ts, 9 * 60, dow, 17, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date-dow-or-dom: next is Mon 20th");
    assert_time_eq("date-dow-or-dom: Mon 20th", t2, make_time_local(2025, 1, 20, 9, 0, 0));

    /* Union scoped by a months mask: Mon OR the 15th, but only in June.
     * From January the first hit is the first June Monday (Jun 2), which is
     * earlier than June 15. */
    esp_schedule_validity_t val_long = { .start_time = 0, .end_time = now + 220L * 24 * 3600 };
    time_t t3 = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, dow, 15, ESP_SCHEDULE_MONTH_JUNE, 0, &val_long, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "date-dow-or-dom: Mon OR 15th in June");
    assert_time_eq("date-dow-or-dom: first June Monday", t3, make_time_local(2025, 6, 2, 9, 0, 0));
}

// --- DOW + months mask with NO day-of-month (day=0): weekday pattern scoped to
//     the masked months, e.g. "every Monday in June", "every Mon/Wed/Fri in June". ---
TEST_CASE("date dow months mask", "[esp_schedule]")
{
    /* From January, year=0 so the pattern recurs. Validity spans ~1.5 years so
     * the wrap into next year's June is reachable. */
    time_t now = make_time_local(2025, 1, 10, 7, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 550L * 24 * 3600 };

    /* Single DOW + months mask: every Monday in June. June 2025 Mondays:
     * 2, 9, 16, 23, 30. First hit from January is Jun 2. */
    time_t t1 = 0;
    bool ok = esp_schedule_get_next_date_time(now, 7 * 60, ESP_SCHEDULE_DAY_MONDAY, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "dow+months: first Monday in June");
    assert_time_eq("dow+months: Jun 2 2025", t1, make_time_local(2025, 6, 2, 7, 0, 0));

    time_t t2 = 0;
    ok = esp_schedule_get_next_date_time(t1, 7 * 60, ESP_SCHEDULE_DAY_MONDAY, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "dow+months: second Monday in June");
    assert_time_eq("dow+months: Jun 9 2025", t2, make_time_local(2025, 6, 9, 7, 0, 0));

    /* From the last June Monday (Jun 30 2025), the months gate forbids any July
     * match -> wrap to the first Monday of June 2026 (Jun 1 2026 is a Monday). */
    time_t last_jun = make_time_local(2025, 6, 30, 7, 0, 0);
    time_t t3 = 0;
    ok = esp_schedule_get_next_date_time(last_jun, 7 * 60, ESP_SCHEDULE_DAY_MONDAY, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "dow+months: wraps to next June");
    assert_time_eq("dow+months: Jun 1 2026", t3, make_time_local(2026, 6, 1, 7, 0, 0));

    /* Multiple DOW + months mask: every Mon/Wed/Fri in June. June 2025:
     * Mon 2, Wed 4, Fri 6, Mon 9, ... First three hits from January. */
    uint8_t mwf = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_FRIDAY;
    time_t m1 = 0;
    ok = esp_schedule_get_next_date_time(now, 7 * 60, mwf, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &m1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "multi-dow+months: first (Mon Jun 2)");
    assert_time_eq("multi-dow+months: Jun 2 2025", m1, make_time_local(2025, 6, 2, 7, 0, 0));

    time_t m2 = 0;
    ok = esp_schedule_get_next_date_time(m1, 7 * 60, mwf, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &m2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "multi-dow+months: second (Wed Jun 4)");
    assert_time_eq("multi-dow+months: Jun 4 2025", m2, make_time_local(2025, 6, 4, 7, 0, 0));

    time_t m3 = 0;
    ok = esp_schedule_get_next_date_time(m2, 7 * 60, mwf, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &m3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "multi-dow+months: third (Fri Jun 6)");
    assert_time_eq("multi-dow+months: Jun 6 2025", m3, make_time_local(2025, 6, 6, 7, 0, 0));
}

// --- EVERYDAY OR'd with a day-of-month subsumes the day-of-month (all days) ---
TEST_CASE("date everyday subsumes dom", "[esp_schedule]")
{
    /* dow=EVERYDAY covers all 7 weekdays, so OR'ing it with day-of-month=15
     * matches EVERY day, not just the 15th. Discriminator: a bare day=15 from
     * Jan 16 would jump to Feb 15; EVERYDAY makes it fire tomorrow (Jan 17). */
    time_t now = make_time_local(2025, 1, 16, 12, 0, 0); // Thu 12:00
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 60L * 24 * 3600 };

    time_t t1 = 0;
    bool ok = esp_schedule_get_next_date_time(now, 6 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 15, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "everyday+dom: first (tomorrow, not next 15th)");
    assert_time_eq("everyday+dom -> Jan 17 (not Feb 15)", t1, make_time_local(2025, 1, 17, 6, 0, 0));

    /* Confirms daily cadence: next occurrence is the following day. */
    time_t t2 = 0;
    ok = esp_schedule_get_next_date_time(t1, 6 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 15, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "everyday+dom: second (next day)");
    assert_time_eq("everyday+dom -> Jan 18", t2, make_time_local(2025, 1, 18, 6, 0, 0));
}

// --- DATE + DOW one-shot semantics mirror day-of-month ---
TEST_CASE("date type dow one shot", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 6, 15, 12, 0, 0);
    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    tr.date.day = 0;
    tr.date.year = 0;
    tr.date.repeat_every_year = false;
    tr.next_scheduled_time_utc = now - 1; /* already fired */

    /* Without repeat_every_year a DATE+DOW trigger is one-shot, same as a bare
     * day-of-month: it fires once on the next matching day, then is done. */
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "DATE+DOW without repeat_every_year -> one-shot");

    /* repeat_every_year makes the DOW pattern recur. */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "DATE+DOW with repeat_every_year -> repeats");
}

// --- Knife edge: now equals target -> should select next occurrence ---
TEST_CASE("knife edge now equals target", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 16, 8, 0, 0); // Thu 08:00
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10 * 24 * 3600 };

    uint8_t days_of_week = (1 << 3); // Thursday
    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 8 * 60, days_of_week, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "knife-edge: now != target (Thu 08:00)");

    // should be the next Thursday at 08:00
    assert_time_eq("knife-edge: now != target (Thu 08:00)", next_ts, make_time_local(2025, 1, 23, 8, 0, 0));
}

// --- Validity window ---
TEST_CASE("validity respected", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 16, 23, 50, 0);
    esp_schedule_validity_t validity = { .start_time = now + 20 * 60, .end_time = now + 2 * 24 * 3600 };

    time_t next_ts = 0; bool ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: start boundary honored");
    assert_time_eq("validity: start boundary honored", next_ts, validity.start_time);

    /* Open-ended window (start==0 && end==0): both bounds disabled -> just the
     * next occurrence, here tomorrow 00:10 since 00:10 already passed today. */
    esp_schedule_validity_t open = { .start_time = 0, .end_time = 0 };
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &open, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: open-ended");
    assert_time_eq("validity: open-ended -> tomorrow 00:10", next_ts, make_time_local(2025, 1, 17, 0, 10, 0));

    /* NULL validity behaves the same as an open-ended window. */
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, NULL, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: NULL");
    assert_time_eq("validity: NULL -> tomorrow 00:10", next_ts, make_time_local(2025, 1, 17, 0, 10, 0));

    /* Each bound is disabled INDEPENDENTLY (not only when both are zero). */

    /* start-only (end_time == 0 disabled): future start honored, no upper cap. */
    esp_schedule_validity_t start_only = { .start_time = now + 20 * 60, .end_time = 0 };
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &start_only, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: start-only honored, end disabled");
    assert_time_eq("validity: start-only -> start_time", next_ts, start_only.start_time);

    /* end-only (start_time == 0 disabled): next occurrence bounded only by end. */
    esp_schedule_validity_t end_only = { .start_time = 0, .end_time = now + 2 * 24 * 3600 };
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &end_only, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "validity: end-only, start disabled");
    assert_time_eq("validity: end-only -> tomorrow 00:10", next_ts, make_time_local(2025, 1, 17, 0, 10, 0));

    /* end-only that cuts off the next occurrence -> no match. 00:10 already passed
     * today (23:50), and tomorrow's 00:10 is past this tight end_time. */
    esp_schedule_validity_t end_tight = { .start_time = 0, .end_time = now + 5 * 60 };
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 10, 0, 0, 0, 0, &end_tight, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "validity: end-only cutoff -> no match");
    TEST_ASSERT_TRUE(next_ts == 0);
}

// --- Sequences for same trigger type ---
TEST_CASE("sequence dow mon wed", "[esp_schedule]")
{
    // Sequence Mon/Wed 09:00 from Monday 08:50 -> Mon 09:00, Wed 09:00, next Mon 09:00
    time_t now = make_time_local(2025, 1, 13, 8, 50, 0); // Monday
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 30 * 24 * 3600 };
    uint8_t days_of_week = (1 << 0) | (1 << 2); // Mon, Wed

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: first");
    assert_time_eq("seq dow: first", t1, make_time_local(2025, 1, 13, 9, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 9 * 60, days_of_week, 0, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: second");
    assert_time_eq("seq dow: second", t2, make_time_local(2025, 1, 15, 9, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 9 * 60, days_of_week, 0, 0, 0, &validity, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq dow: third");
    assert_time_eq("seq dow: third", t3, make_time_local(2025, 1, 20, 9, 0, 0));
}

TEST_CASE("sequence date months mask", "[esp_schedule]")
{
    // Day=15 at 07:00 for months {Jan, Mar, Apr}
    time_t now = make_time_local(2025, 1, 10, 7, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 370 * 24 * 3600 };
    uint16_t months = (1u << 0) | (1u << 2) | (1u << 3); // Jan, Mar, Apr

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, months, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: first");
    assert_time_eq("seq date: first", t1, make_time_local(2025, 1, 15, 7, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 7 * 60, 0, 15, months, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: second");
    assert_time_eq("seq date: second", t2, make_time_local(2025, 3, 15, 7, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 7 * 60, 0, 15, months, 0, &validity, &t3);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: third");
    assert_time_eq("seq date: third", t3, make_time_local(2025, 4, 15, 7, 0, 0));

    // year=0 => the masked-month pattern wraps into the next year (repeats yearly),
    // it does NOT expire after the last masked month of the current year.
    time_t t4 = 0; ok = esp_schedule_get_next_date_time(t3, 7 * 60, 0, 15, months, 0, &validity, &t4);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq date: fourth wraps to next year");
    assert_time_eq("seq date: fourth -> Jan 15 2026", t4, make_time_local(2026, 1, 15, 7, 0, 0));
}

TEST_CASE("sequence validity cutoff", "[esp_schedule]")
{
    // Validity end should stop sequences
    time_t now = make_time_local(2025, 1, 13, 8, 50, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_local(2025, 1, 16, 0, 0, 0) };
    uint8_t days_of_week = (1 << 0) | (1 << 2); // Mon, Wed

    time_t t1 = 0; bool ok = esp_schedule_get_next_date_time(now, 9 * 60, days_of_week, 0, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq cutoff: first");
    assert_time_eq("seq cutoff: first", t1, make_time_local(2025, 1, 13, 9, 0, 0));

    time_t t2 = 0; ok = esp_schedule_get_next_date_time(t1, 9 * 60, days_of_week, 0, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "seq cutoff: second");
    assert_time_eq("seq cutoff: second", t2, make_time_local(2025, 1, 15, 9, 0, 0));

    time_t t3 = 0; ok = esp_schedule_get_next_date_time(t2, 9 * 60, days_of_week, 0, 0, 0, &validity, &t3);
    TEST_ASSERT_FALSE_MESSAGE(ok, "seq cutoff: third should fail due to validity end");
    TEST_ASSERT_TRUE(t3 == 0);
}

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
TEST_CASE("solar with dow", "[esp_schedule]")
{
    double lat = 37.7749, lon = -122.4194; // San Francisco, CA
    time_t now = make_time_local(2025, 1, 12, 6, 0, 0);
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
    time_t now = make_time_local(2025, 6, 15, 12, 0, 0);
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
    time_t now = make_time_local(year, month, day, 0, 0, 0);
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

// --- Solar one-shot semantics must match DATE exactly ---
TEST_CASE("solar one shot matches date", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 6, 15, 12, 0, 0);
    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
    tr.next_scheduled_time_utc = now - 1; /* already fired */

    /* Bare solar (no pattern) -> one-shot, same as an all-wildcard DATE. */
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar bare -> one-shot");

    /* Day-of-week only, no repeat_every_year -> one-shot (mirrors DATE). */
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar DOW without repeat_every_year -> one-shot");

    /* repeat_every_year -> repeats. */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar repeat_every_year -> repeats");

    /* A months mask keeps it firing through the masked months (not one-shot). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_SUNSET;
    tr.date.day = 15;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar with months mask -> not one-shot");

    /* Day-of-month only, no year/months/repeat_every_year -> one-shot (fires the
     * next 15th's solar event, then done), same as DATE. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
    tr.date.day = 15;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar day-of-month without repeat_every_year -> one-shot");
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar day-of-month + repeat_every_year -> repeats");
}

// --- Solar day-selection variants: daily, weekend, monthly, offset, year bound ---
TEST_CASE("solar variants", "[esp_schedule]")
{
    const double lat = 37.7749, lon = -122.4194; // San Francisco, CA
    time_t sr = 0, ss = 0;

    /* Every day at the exact sunrise (repeat_days = 0, repeat_every_year). */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        time_t prev = now;
        for (int d = 12; d <= 14; d++) {
            TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, d, lat, lon, &sr, &ss));
            time_t got = esp_schedule_get_next_valid_solar_time(prev, &tr, &val, "daily");
            assert_time_eq("solar daily sunrise", got, sr);
            prev = got;
        }
        /* EVERYDAY must select the same first day as the wildcard. */
        esp_schedule_trigger_t tr_every = tr;
        tr_every.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
        time_t a = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "wild");
        time_t b = esp_schedule_get_next_valid_solar_time(now, &tr_every, &val, "every");
        assert_time_eq("solar EVERYDAY == wildcard", b, a);
    }

    /* Weekends only (Sat|Sun). Jan 12 2025 is a Sunday. */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.day.repeat_days = ESP_SCHEDULE_DAY_SATURDAY | ESP_SCHEDULE_DAY_SUNDAY;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        const int weekend_days[] = { 12, 18, 19 }; // Sun, Sat, Sun
        time_t prev = now;
        for (int i = 0; i < 3; i++) {
            TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, weekend_days[i], lat, lon, &sr, &ss));
            time_t got = esp_schedule_get_next_valid_solar_time(prev, &tr, &val, "weekend");
            assert_time_eq("solar weekend sunrise", got, sr);
            prev = got;
        }
    }

    /* 15th of every month at sunset (day-of-month + repeat_every_year). */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 60L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNSET;
        tr.date.day = 15;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 15, lat, lon, &sr, &ss));
        time_t t1 = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "monthly");
        assert_time_eq("solar 15th sunset (Jan)", t1, ss);
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 2, 15, lat, lon, &sr, &ss));
        time_t t2 = esp_schedule_get_next_valid_solar_time(t1, &tr, &val, "monthly");
        assert_time_eq("solar 15th sunset (Feb)", t2, ss);
    }

    /* Offset: 15 minutes after sunrise, every day. */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        tr.solar.offset_minutes = 15;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 12, lat, lon, &sr, &ss));
        time_t got = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "offset");
        assert_time_eq("solar sunrise +15min", got, sr + 15 * 60);
    }

    /* Year-bounded: every Monday's sunrise, in 2026 only. Jan 5 2026 is a Monday. */
    {
        time_t now = make_time_local(2025, 6, 1, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = make_time_local(2027, 1, 1, 0, 0, 0) };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
        tr.date.year = 2026;
        tr.date.repeat_every_year = false;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2026, 1, 5, lat, lon, &sr, &ss));
        time_t got = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "yearbound");
        assert_time_eq("solar year-bounded first Monday 2026", got, sr);
        struct tm lt; localtime_r(&got, &lt);
        TEST_ASSERT_EQUAL_INT_MESSAGE(2026 - 1900, lt.tm_year, "solar year-bounded must be in 2026");

        /* Once that year is in the past, there is no further occurrence. */
        time_t past = make_time_local(2027, 2, 1, 0, 0, 0);
        time_t none = esp_schedule_get_next_valid_solar_time(past, &tr, &val, "yearbound-expired");
        TEST_ASSERT_TRUE_MESSAGE(none == 0, "solar year-bounded expires after its year");
    }

    /* Union: sunrise on Monday OR the 15th. Jan 2025: Mon 13 comes before the
     * 15th, so the first hit is Jan 13; the next is Jan 15. */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0); // Sun
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 40L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
        tr.date.day = 15;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 13, lat, lon, &sr, &ss));
        time_t t1 = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "union");
        assert_time_eq("solar union -> Mon Jan 13", t1, sr);
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 15, lat, lon, &sr, &ss));
        time_t t2 = esp_schedule_get_next_valid_solar_time(t1, &tr, &val, "union");
        assert_time_eq("solar union -> 15th", t2, sr);
    }

    /* Wildcard day + months mask -> the solar event every day within the mask.
     * From late May the first hit is Jun 1, then Jun 2. */
    {
        time_t now = make_time_local(2025, 5, 30, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 60L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNSET;
        tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 6, 1, lat, lon, &sr, &ss));
        time_t t1 = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "daysinmonth");
        assert_time_eq("solar every-day-in-June -> Jun 1", t1, ss);
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 6, 2, lat, lon, &sr, &ss));
        time_t t2 = esp_schedule_get_next_valid_solar_time(t1, &tr, &val, "daysinmonth");
        assert_time_eq("solar every-day-in-June -> Jun 2", t2, ss);
    }
}
#endif

/* --- esp_schedule_init must not dereference a NULL schedule_count --- */
TEST_CASE("init null schedule_count", "[esp_schedule]")
{
    /* esp_schedule_init() runs timesync (SNTP), which needs the TCP/IP stack up.
     * On a device this is already initialized; bring it up here for the test. */
    esp_netif_init();

    /* NVS-off path used to write *schedule_count unconditionally. */
    esp_schedule_handle_t *h = esp_schedule_init(false, NULL, NULL);
    TEST_ASSERT_NULL_MESSAGE(h, "init(false, NULL, NULL) should return NULL and not crash");

    /* NVS-on path with NULL count is rejected. */
    h = esp_schedule_init(true, NULL, NULL);
    TEST_ASSERT_NULL_MESSAGE(h, "init(true, NULL, NULL) should return NULL and not crash");
}

/* --- one-shot triggers must be detected as fired-and-done --- */
TEST_CASE("one-shot fired and done", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 6, 15, 12, 0, 0);
    esp_schedule_trigger_t tr;

    /* Not computed yet -> never "done". */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_ONCE;
    tr.next_scheduled_time_utc = 0;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "uncomputed -> not done");

    /* DAY_ONCE fired in the past -> done (must not re-arm daily). */
    tr.next_scheduled_time_utc = now - 10;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DAY_ONCE fired -> done");

    /* Still in the future -> not done. */
    tr.next_scheduled_time_utc = now + 10;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "future -> not done");

    /* Repeating weekday -> never done, even after firing. */
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    tr.next_scheduled_time_utc = now - 10;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "repeating weekday -> not done");

    /* RELATIVE fires exactly once. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_RELATIVE;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "RELATIVE fired -> done");

    /* DATE without a year and without repeat_every_year -> one-shot. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    tr.date.year = 0;
    tr.date.repeat_every_year = false;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE yearless one-shot -> done");

    /* DATE repeat_every_year -> recurs. */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE repeat_every_year -> not done");

    /* DATE bound to a specific year -> engine bounds it, not "done" here. */
    tr.date.repeat_every_year = false;
    tr.date.year = 2025;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE year-bounded handled by engine");

    /* DATE with a months mask (no year, no repeat_every_year) -> NOT one-shot:
     * it must keep firing through the remaining masked months. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 16;
    tr.date.year = 0;
    tr.date.repeat_every_year = false;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY | ESP_SCHEDULE_MONTH_AUGUST;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE with months mask -> not one-shot");

    /* DATE all-wildcard (no day/dow/months/year) -> one-shot; repeat_every_year
     * turns it into a daily schedule. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE all-wildcard -> one-shot");
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE all-wildcard + repeat_every_year -> daily");

    /* EVERYDAY behaves like the wildcard for DATE: the day mask does NOT gate
     * repetition, so it is one-shot unless repeat_every_year is set. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE EVERYDAY -> one-shot");
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE EVERYDAY + repeat_every_year -> daily");

    /* Day-of-month with repeat_every_year -> fires that day every month, forever. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    tr.date.repeat_every_year = true;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE 15th + repeat_every_year -> monthly forever");
}

/* --- repeat_every_year / year bounding in the date engine --- */
TEST_CASE("date year bounding", "[esp_schedule]")
{
    time_t now = make_time_local(2026, 3, 1, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 800L * 24 * 3600 };
    time_t next_ts = 0;

    /* A year in the past yields no match (bounds a non-repeating date). */
    bool ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 10, 0, 2025, &validity, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "year=2025 in the past -> no match");

    /* year=0 (repeat_every_year semantics) recurs to the next occurrence. */
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 10, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "year=0 -> matches next occurrence");
    assert_time_eq("year=0 day=10 09:00", next_ts, make_time_local(2026, 3, 10, 9, 0, 0));

    /* Day-only + a FUTURE specific year: fires that year's occurrences, then the
     * engine bounds it (no match once the year is in the past). Covers §4.2 row
     * "5 / 0 / 2026 / false" -> fires the 5th of each month in 2026 only, then dead. */
    time_t before = make_time_local(2025, 1, 16, 12, 0, 0);
    esp_schedule_validity_t wide = { .start_time = 0, .end_time = make_time_local(2028, 1, 1, 0, 0, 0) };
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(before, 9 * 60 + 15, 0, 5, 0, 2026, &wide, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "day=5 year=2026 -> first fire in 2026");
    assert_time_eq("day=5 year=2026 -> Jan 5 2026", next_ts, make_time_local(2026, 1, 5, 9, 15, 0));

    /* Same config once 2026 is in the past -> no further occurrence (expired). */
    time_t after = make_time_local(2027, 2, 1, 12, 0, 0);
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(after, 9 * 60 + 15, 0, 5, 0, 2026, &wide, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "day=5 year=2026 -> expires after 2026");
    TEST_ASSERT_TRUE(next_ts == 0);
}

/* --- wildcard-day, EVERYDAY, monthly and every-day-in-month selection --- */
TEST_CASE("date wildcard and daily", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 16, 12, 0, 0); // Thu 12:00
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 400L * 24 * 3600 };
    time_t next_ts = 0;

    /* All wildcards, time already passed today -> fires tomorrow at 06:00. */
    bool ok = esp_schedule_get_next_date_time(now, 6 * 60, 0, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "wildcard 06:00");
    assert_time_eq("wildcard -> tomorrow 06:00", next_ts, make_time_local(2025, 1, 17, 6, 0, 0));

    /* All wildcards, time still ahead today -> fires today at 15:00. */
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 15 * 60, 0, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "wildcard 15:00");
    assert_time_eq("wildcard -> today 15:00", next_ts, make_time_local(2025, 1, 16, 15, 0, 0));

    /* EVERYDAY mask is equivalent to a wildcard day. */
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 6 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "EVERYDAY 06:00");
    assert_time_eq("EVERYDAY -> tomorrow 06:00 (== wildcard)", next_ts, make_time_local(2025, 1, 17, 6, 0, 0));

    /* Day-of-month only: the 15th already passed this month -> next month's 15th. */
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "day=15 monthly");
    assert_time_eq("day=15 -> Feb 15 07:00", next_ts, make_time_local(2025, 2, 15, 7, 0, 0));

    /* Wildcard day + a months mask -> every day in that month; first hit Jun 1. */
    next_ts = 0; ok = esp_schedule_get_next_date_time(now, 8 * 60, 0, 0, ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "day=0 months=Jun");
    assert_time_eq("day=0 + Jun -> Jun 1 08:00", next_ts, make_time_local(2025, 6, 1, 8, 0, 0));
}

/* --- A months mask bound to a specific year expires after its last masked
 * month (v1.3.3 is_expired semantics: fire the year's masked months, then stop). */
TEST_CASE("date months year bounded expires", "[esp_schedule]")
{
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_local(2027, 1, 1, 0, 0, 0) };
    uint16_t months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;

    time_t t1 = 0;
    bool ok = esp_schedule_get_next_date_time(make_time_local(2025, 1, 1, 0, 0, 0), 7 * 60, 0, 15, months, 2025, &validity, &t1);
    TEST_ASSERT_TRUE_MESSAGE(ok, "bounded months: first");
    assert_time_eq("bounded months -> Jun 15 2025", t1, make_time_local(2025, 6, 15, 7, 0, 0));

    time_t t2 = 0;
    ok = esp_schedule_get_next_date_time(t1, 7 * 60, 0, 15, months, 2025, &validity, &t2);
    TEST_ASSERT_TRUE_MESSAGE(ok, "bounded months: second");
    assert_time_eq("bounded months -> Jul 15 2025", t2, make_time_local(2025, 7, 15, 7, 0, 0));

    /* Past the last masked month of the bound year -> no more occurrences. */
    time_t t3 = 0;
    ok = esp_schedule_get_next_date_time(t2, 7 * 60, 0, 15, months, 2025, &validity, &t3);
    TEST_ASSERT_FALSE_MESSAGE(ok, "bounded months: expires after last masked month");
    TEST_ASSERT_TRUE(t3 == 0);
}

/* --- DST must not be double-corrected --- */
#define NY_TZ "EST5EDT,M3.2.0,M11.1.0" /* America/New_York */

/* TZ guard so tests restore the previous value even if a Unity assertion
 * longjmps past the test's own tz_pop(). */
static bool s_tz_active = false;
static bool s_tz_had = false;
static char s_tz_saved[64];

static void tz_push(const char *tz)
{
    const char *cur = getenv("TZ");
    s_tz_had = (cur != NULL);
    if (cur) {
        strlcpy(s_tz_saved, cur, sizeof(s_tz_saved));
    }
    setenv("TZ", tz, 1);
    tzset();
    s_tz_active = true;
}

static void tz_pop(void)
{
    if (!s_tz_active) {
        return;
    }
    if (s_tz_had) {
        setenv("TZ", s_tz_saved, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    s_tz_active = false;
}

TEST_CASE("dst daily across spring forward", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* Spring forward 2025: Sun Mar 9. Daily 12:00 from Sat Mar 8 13:00. */
    time_t now = make_time_local(2025, 3, 8, 13, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10L * 24 * 3600 };
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 12 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE(ok);
    assert_time_eq("spring-forward daily 12:00", next_ts, make_time_local(2025, 3, 9, 12, 0, 0));

    tz_pop();
}

TEST_CASE("dst daily across fall back", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* Fall back 2025: Sun Nov 2. Daily 12:00 from Sat Nov 1 13:00. */
    time_t now = make_time_local(2025, 11, 1, 13, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10L * 24 * 3600 };
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 12 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE(ok);
    assert_time_eq("fall-back daily 12:00", next_ts, make_time_local(2025, 11, 2, 12, 0, 0));

    tz_pop();
}

TEST_CASE("dst skipped local time", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* 02:30 on spring-forward day does not exist (clocks jump 02:00->03:00).
     * mktime(tm_isdst=-1) must resolve it to a real future instant, not drift. */
    time_t now = make_time_local(2025, 3, 9, 1, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 2L * 24 * 3600 };
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 2 * 60 + 30, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE_MESSAGE(next_ts > now, "skipped local time resolves to a future instant");
    TEST_ASSERT_TRUE_MESSAGE(next_ts < now + 4 * 3600, "resolved within a few hours, no day/hour drift");

    tz_pop();
}

/* --- advancing days must not skip a day across spring forward --- */
TEST_CASE("dst next dow near midnight", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* Sat Mar 8 2025 23:30, next Sunday 23:45 -> Sun Mar 9 (NOT Mar 16).
     * The old code advanced by 86400s and skipped the 23h DST day. */
    time_t now = make_time_local(2025, 3, 8, 23, 30, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 23 * 60 + 45, ESP_SCHEDULE_DAY_SUNDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE(ok);
    assert_time_eq("next Sunday 23:45 across spring-forward", next_ts, make_time_local(2025, 3, 9, 23, 45, 0));

    tz_pop();
}

/* --- RainMaker DST compliance ---
 * https://legacy.rainmaker.espressif.com/docs/scheduling/#managing-daylight-saving-time-dst
 * Schedules fire on LOCAL wall-clock time:
 *  - Spring forward: a 02:00-02:59 local time does not exist; the schedule is
 *    delayed by 1hr and fires at 03:00-03:59 on the switch day.
 *  - Fall back: a 01:00-01:59 local time occurs twice; the schedule fires only
 *    once (the first occurrence, before the switch), not again after it. */
TEST_CASE("dst rainmaker spring forward delayed one hour", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* Daily 02:30. On spring-forward day (Sun Mar 9 2025) 02:30 does not exist,
     * so it must fire at 03:30. From Sat Mar 8 12:00 the next occurrence is Mar 9. */
    time_t now = make_time_local(2025, 3, 8, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10L * 24 * 3600 };
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 2 * 60 + 30, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE(ok);
    /* 02:30 Mar 9 resolves to the same instant as 03:30 EDT. */
    assert_time_eq("spring 02:30 -> 03:30", next_ts, make_time_local(2025, 3, 9, 3, 30, 0));
    struct tm lt;
    localtime_r(&next_ts, &lt);
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, lt.tm_hour, "must fire in the 03:00-03:59 window (delayed 1hr)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, lt.tm_mday, "must fire on the switch day, not skip to the next");

    tz_pop();
}

TEST_CASE("dst rainmaker fall back fires once", "[esp_schedule]")
{
    tz_push(NY_TZ);

    /* Daily 01:30. On fall-back day (Sun Nov 2 2025) 01:30 occurs twice. It must
     * fire once at the first (EDT) occurrence and then advance to the next day,
     * NOT fire again at the repeated 01:30 EST. */
    time_t now = make_time_local(2025, 11, 1, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = now + 10L * 24 * 3600 };

    time_t t1 = 0;
    bool ok = esp_schedule_get_next_date_time(now, 1 * 60 + 30, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &t1);
    TEST_ASSERT_TRUE(ok);
    assert_time_eq("fall 01:30 first occurrence (EDT)", t1, make_time_local(2025, 11, 2, 1, 30, 0));

    /* Next occurrence must be the next day, not the repeated 01:30 EST. */
    time_t t2 = 0;
    ok = esp_schedule_get_next_date_time(t1, 1 * 60 + 30, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &t2);
    TEST_ASSERT_TRUE(ok);
    assert_time_eq("fall next occurrence is next day", t2, make_time_local(2025, 11, 3, 1, 30, 0));
    TEST_ASSERT_TRUE_MESSAGE((t2 - t1) >= 24 * 3600, "advanced a full day, not the repeated DST hour");

    tz_pop();
}

/* Unity runs setUp()/tearDown() before/after every TEST_CASE. tearDown()
 * restores TZ even if a DST test aborted via a failed assertion before its own
 * tz_pop(), so a leaked TZ cannot make later timezone-independent tests fail. */
void setUp(void)
{
}

void tearDown(void)
{
    tz_pop();
}
