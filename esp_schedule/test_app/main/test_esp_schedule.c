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

    /* Worst satisfiable case for the search horizon (§1.2 step 6): 2100 is not a
     * leap year, so from Mar 2096 the next Feb 29 is 2104 - an 8-year gap, which
     * the month-attempt budget must still cover. Needs a 64-bit time_t. */
    if (sizeof(time_t) > 4) {
        time_t far = make_time_local(2096, 3, 1, 12, 0, 0);
        esp_schedule_validity_t wide = { .start_time = 0, .end_time = 0 };
        next_ts = 0;
        ok = esp_schedule_get_next_date_time(far, 9 * 60, 0, 29, ESP_SCHEDULE_MONTH_FEBRUARY, 0, &wide, &next_ts);
        TEST_ASSERT_TRUE_MESSAGE(ok, "date: Feb 29 across the 8-year leap gap");
        assert_time_eq("date: Feb 29 -> 2104-02-29 09:00", next_ts, make_time_local(2104, 2, 29, 9, 0, 0));
    }
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

// --- Engine-internal: the day-of-week / day-of-month union ---
// NOTE: no trigger configuration can reach this. Both day arms set is rejected
// (rule V2) for DATE and for solar, so the engine's OR of the two arms is dead
// code from the caller's point of view. This test pins the engine contract only;
// the rejection of these shapes is covered by "trigger validation truth
// table".
TEST_CASE("date type dow or dom", "[esp_schedule]")
{
    /* Thu Jan 16 2025 07:45. Passing both a DOW mask (Mon) and a day-of-month
     * (17th) selects the union: the nearer of next Mon or the 17th. From Thu,
     * the 17th (Fri) comes before Mon the 20th. */
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

// --- Engine-internal: DOW + months mask with NO day-of-month (day=0), e.g.
//     "every Monday in June". No trigger can reach this either: a DOW mask with
//     any date field set is rejected (rule V2), and there is no month-scoped
//     weekday shape in the API. Engine contract only. ---
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

// --- Engine-internal: EVERYDAY OR'd with a day-of-month subsumes the
//     day-of-month (all days). Unreachable from a trigger (rule V2) — this was
//     the footgun that motivated making the two arms exclusive. ---
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

// --- DATE has no day-of-week arm: a weekday pattern belongs to DAYS_OF_WEEK ---
TEST_CASE("date type dow rejected", "[esp_schedule]")
{
    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "date_dow"),
                              "DATE with day.repeat_days -> V2");

    /* Adding a date field does not make it legal either - the arms are exclusive. */
    tr.date.day = 15;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "date_dow_dom"),
                              "DATE with both arms -> V2");

    /* The intended spelling of "every Monday" is DAYS_OF_WEEK with no date field. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "dow"), "DOW-1 is valid");
    tr.next_scheduled_time_utc = make_time_local(2025, 6, 15, 12, 0, 0) - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, make_time_local(2025, 6, 15, 12, 0, 0)),
                              "DOW-1 repeats forever");
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

    // Engine-level: year=0 leaves the year unconstrained, so the masked-month
    // pattern wraps into the next year. A DATE-3 trigger does NOT reach this - the
    // arm path passes the current year (see "date months mask year scoping"); only
    // DATE-4 (repeat_every_year) passes 0 and wraps like this.
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

/* A DATE-3 shape (day-of-month + months mask, no year and no repeat_every_year):
 * bounded to the current year, so it fires on the 15th of each masked month of
 * 2025 - exactly the sequence asserted below. */
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

    /* Bare solar (all-wildcard date arm) -> one-shot, same as an all-wildcard
     * DATE (DATE-0). */
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar bare -> one-shot");

    /* Day-of-week arm: solar's only weekday shape, and it repeats forever
     * (SOL-0). This is where solar differs from DATE, which has no DOW arm. */
    tr.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "solar_dow"), "SOL-0 is valid");
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar DOW arm -> repeats forever");

    /* A months mask without year or repeat_every_year recurs through the masked
     * months of the current year (DATE-3). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_SUNSET;
    tr.date.day = 15;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar months mask alone -> recurs within the current year (DATE-3)");

    /* repeat_every_year over that mask makes it recur forever (DATE-4). */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar months mask + repeat_every_year -> repeats (DATE-4)");

    /* A year over the mask recurs through that year, then the engine bounds it
     * (DATE-5). */
    tr.date.repeat_every_year = false;
    tr.date.year = 2025;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                              "solar months mask + year -> recurs within the year (DATE-5)");

    /* Day-of-month alone -> one-shot on the next 15th's solar event (DATE-1). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
    tr.date.day = 15;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar day-of-month alone -> one-shot (DATE-1)");

    /* Adding a year but no mask stays one-shot (DATE-2); repeat_every_year with no
     * mask is not a schedule at all (V6). */
    tr.date.year = 2025;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now),
                             "solar day-of-month + year, no mask -> one-shot (DATE-2)");
    tr.date.year = 0;
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "solar_ry_nomask"),
                              "solar repeat_every_year with no months mask -> V6");
}

// --- Solar day-selection variants: daily, weekend, monthly, offset, year bound ---
TEST_CASE("solar variants", "[esp_schedule]")
{
    const double lat = 37.7749, lon = -122.4194; // San Francisco, CA
    time_t sr = 0, ss = 0;

    /* Wildcard day arm (DATE-0): the engine walks day by day. The trigger itself
     * is one-shot, so this pins the day-selection walk only. */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "DATE-0"));
        time_t prev = now;
        for (int d = 12; d <= 14; d++) {
            TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, d, lat, lon, &sr, &ss));
            time_t got = esp_schedule_get_next_valid_solar_time(prev, &tr, &val, "daily");
            assert_time_eq("solar daily sunrise", got, sr);
            prev = got;
        }
        /* The EVERYDAY day-of-week arm (SOL-0) selects the same first day as the
         * wildcard, and is the way to spell "every day at sunrise". */
        esp_schedule_trigger_t tr_every = tr;
        tr_every.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
        TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr_every, "SOL-0"));
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
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "SOL-0 weekend"));
        const int weekend_days[] = { 12, 18, 19 }; // Sun, Sat, Sun
        time_t prev = now;
        for (int i = 0; i < 3; i++) {
            TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, weekend_days[i], lat, lon, &sr, &ss));
            time_t got = esp_schedule_get_next_valid_solar_time(prev, &tr, &val, "weekend");
            assert_time_eq("solar weekend sunrise", got, sr);
            prev = got;
        }
    }

    /* 15th of every month at sunset (DATE-4: day-of-month + full month mask +
     * repeat_every_year). The mask is what makes it recur; repeat_every_year with
     * no mask is rejected (V6). */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 60L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNSET;
        tr.date.day = 15;
        tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
        tr.date.repeat_every_year = true;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "DATE-4"));
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 15, lat, lon, &sr, &ss));
        time_t t1 = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "monthly");
        assert_time_eq("solar 15th sunset (Jan)", t1, ss);
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 2, 15, lat, lon, &sr, &ss));
        time_t t2 = esp_schedule_get_next_valid_solar_time(t1, &tr, &val, "monthly");
        assert_time_eq("solar 15th sunset (Feb)", t2, ss);
    }

    /* Offset: 15 minutes after sunrise, every day (SOL-0 with EVERYDAY). */
    {
        time_t now = make_time_local(2025, 1, 12, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = now + 30L * 24 * 3600 };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        tr.solar.offset_minutes = 15;
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2025, 1, 12, lat, lon, &sr, &ss));
        time_t got = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "offset");
        assert_time_eq("solar sunrise +15min", got, sr + 15 * 60);
    }

    /* Year-bounded (DATE-5): the 5th of every month in 2026 only. Jan 5 2026 is
     * the first occurrence. A solar day-of-week pattern cannot be year-bounded
     * (V2) - a date arm is the only year-bounded shape. */
    {
        time_t now = make_time_local(2025, 6, 1, 0, 0, 0);
        esp_schedule_validity_t val = { .start_time = 0, .end_time = make_time_local(2027, 1, 1, 0, 0, 0) };
        esp_schedule_trigger_t tr; memset(&tr, 0, sizeof(tr));
        tr.type = ESP_SCHEDULE_TYPE_SUNRISE;
        tr.date.day = 5;
        tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
        tr.date.year = 2026;
        tr.date.repeat_every_year = false;
        tr.solar.latitude = lat; tr.solar.longitude = lon;
        TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "DATE-5"));
        TEST_ASSERT_TRUE(esp_daylight_calc_sunrise_sunset_utc(2026, 1, 5, lat, lon, &sr, &ss));
        time_t got = esp_schedule_get_next_valid_solar_time(now, &tr, &val, "yearbound");
        assert_time_eq("solar year-bounded first occurrence 2026", got, sr);
        struct tm lt; localtime_r(&got, &lt);
        TEST_ASSERT_EQUAL_INT_MESSAGE(2026 - 1900, lt.tm_year, "solar year-bounded must be in 2026");

        /* Once that year is in the past, there is no further occurrence. */
        time_t past = make_time_local(2027, 2, 1, 0, 0, 0);
        time_t none = esp_schedule_get_next_valid_solar_time(past, &tr, &val, "yearbound-expired");
        TEST_ASSERT_TRUE_MESSAGE(none == 0, "solar year-bounded expires after its year");
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

    /* DATE-1: day-of-month alone -> one-shot. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    tr.date.year = 0;
    tr.date.repeat_every_year = false;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-1 -> done");

    /* DATE-4: the month mask is what repeat_every_year recurs over. Without it,
     * repeat_every_year is rejected outright (V6). */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "V6"), "V6: rep_yr with no month mask -> V6");
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "DATE-4"), "DATE-4 is valid");
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-4 -> not done");

    /* DATE-2: a year with no month mask has nothing to recur over -> one-shot. */
    tr.date.repeat_every_year = false;
    tr.date.repeat_months = 0;
    tr.date.year = 2025;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "DATE-2"), "DATE-2 is valid");
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-2 -> done");

    /* DATE-5: the same year over a month mask recurs through that year's masked
     * months; the engine then bounds it. */
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-5 -> not done");

    /* DATE-3: a months mask with neither year nor repeat_every_year is bounded to
     * the *current* year, so it recurs through that year's masked months. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 16;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY | ESP_SCHEDULE_MONTH_AUGUST;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-3 -> not done");
    TEST_ASSERT_EQUAL_MESSAGE(2025, esp_schedule_date_arm_match_year(&tr, now), "DATE-3 is bound to the current year");

    /* DATE-0: all-wildcard DATE -> one-shot at the next HH:MM. Adding
     * repeat_every_year gives it nothing to recur over (V4). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "DATE-0"), "DATE-0 is valid");
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-0 -> done");
    tr.date.repeat_every_year = true;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "rep_yr_no_pattern"), "all-wildcard + rep_yr -> V4");

    /* DATE has no day-of-week arm at all: EVERYDAY is rejected, not ignored.
     * Daily is DAYS_OF_WEEK + EVERYDAY (DOW-3). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "V2"), "DATE + EVERYDAY -> V2");
}

/* --- Exhaustive configuration truth table ---
 *
 * The two helpers below transcribe the tables in docs/trigger_rules.md §4
 * directly, so
 * they are an independent statement of the rules rather than a paraphrase of the
 * implementation. Every one of the 32 (dow, dom, mon, yr, ry) combinations is
 * checked for each type: whether it is accepted at all (the I rules) and, when
 * accepted, whether it re-arms after firing (the T engine). */

/* Accepted per docs/trigger_rules.md §4. Booleans are "field is non-zero". */
static bool shape_is_valid(esp_schedule_type_t type, bool dow, bool dom, bool mon, bool yr, bool ry)
{
    bool date_arm = dom || mon || yr || ry;
    if (type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        return !date_arm;                           /* V1: DOW-0, DOW-1 */
    }
    bool solar = (type != ESP_SCHEDULE_TYPE_DATE);
    if (dow) {
        /* Solar picks the day-of-week arm only when the date arm is empty
         * (SOL-0); DATE has no day-of-week arm at all. */
        return solar && !date_arm;
    }
    if (!dom && mon) {
        return false;                               /* V3 */
    }
    if (!dom && !mon && (yr || ry)) {
        return false;                               /* V4 */
    }
    if (yr && ry) {
        return false;                               /* V5 */
    }
    if (ry && !mon) {
        return false;                               /* V6 */
    }
    return true;                                    /* DATE-0 .. DATE-5 */
}

/* Re-arms after firing per docs/trigger_rules.md §3. Only called for accepted shapes. */
static bool shape_repeats(esp_schedule_type_t type, bool dow, bool dom, bool mon, bool yr, bool ry)
{
    (void)dom;
    (void)yr;
    (void)ry;
    if (type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        return dow;                                 /* DOW-1 repeats, DOW-0 does not */
    }
    if (type != ESP_SCHEDULE_TYPE_DATE && dow) {
        return true;                                /* SOL-0 */
    }
    /* §3: the month mask alone decides. How far it recurs depends on year /
     * repeat_every_year (forever, the named year, or the current year), but a
     * masked date arm always re-arms. */
    return mon;
}

static void check_all_shapes(esp_schedule_type_t type, const char *type_name, int expect_valid)
{
    time_t now = make_time_local(2025, 6, 15, 12, 0, 0);
    int valid_count = 0;
    char msg[128];

    for (int bits = 0; bits < 32; bits++) {
        bool dow = bits & 1, dom = bits & 2, mon = bits & 4, yr = bits & 8, ry = bits & 16;
        esp_schedule_trigger_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.type = type;
        tr.day.repeat_days = dow ? ESP_SCHEDULE_DAY_MONDAY : 0;
        tr.date.day = dom ? 15 : 0;
        tr.date.repeat_months = mon ? ESP_SCHEDULE_MONTH_ALL : 0;
        tr.date.year = yr ? 2025 : 0;
        tr.date.repeat_every_year = ry;

        bool want_valid = shape_is_valid(type, dow, dom, mon, yr, ry);
        snprintf(msg, sizeof(msg), "%s dow=%d dom=%d mon=%d yr=%d ry=%d: validity", type_name, dow, dom, mon, yr, ry);
        TEST_ASSERT_EQUAL_MESSAGE(want_valid, esp_schedule_trigger_is_valid(&tr, "shape"), msg);
        if (!want_valid) {
            continue;
        }
        valid_count++;

        /* Accepted shapes: an already-fired trigger is "done" iff it does not repeat. */
        tr.next_scheduled_time_utc = now - 1;
        snprintf(msg, sizeof(msg), "%s dow=%d dom=%d mon=%d yr=%d ry=%d: lifetime", type_name, dow, dom, mon, yr, ry);
        TEST_ASSERT_EQUAL_MESSAGE(!shape_repeats(type, dow, dom, mon, yr, ry),
                                  esp_schedule_trigger_fired_and_done(&tr, now), msg);
    }
    snprintf(msg, sizeof(msg), "%s: number of valid shapes out of 32", type_name);
    TEST_ASSERT_EQUAL_MESSAGE(expect_valid, valid_count, msg);
}

TEST_CASE("trigger validation truth table", "[esp_schedule]")
{
    /* Counts per docs/trigger_rules.md §4.7: 2 of 32 for DAYS_OF_WEEK, 6 for
     * DATE, 7 for solar. */
    check_all_shapes(ESP_SCHEDULE_TYPE_DAYS_OF_WEEK, "DAYS_OF_WEEK", 2);
    check_all_shapes(ESP_SCHEDULE_TYPE_DATE, "DATE", 6);
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    check_all_shapes(ESP_SCHEDULE_TYPE_SUNRISE, "SUNRISE", 7);
    check_all_shapes(ESP_SCHEDULE_TYPE_SUNSET, "SUNSET", 7);
#endif

    /* RELATIVE reads no date field, so nothing there can invalidate it. */
    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_RELATIVE;
    tr.relative_seconds = 30;
    tr.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
    tr.date.day = 15;
    tr.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
    tr.date.year = 2025;
    tr.date.repeat_every_year = true;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "relative"), "RELATIVE ignores date fields");

    /* The one field RELATIVE does own must name a future instant (V7). A delay of
     * 0 or less puts the target at or before the base time, which the arm path
     * cannot fire. */
    tr.relative_seconds = 0;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "relative_zero"), "relative_seconds = 0 -> V7");
    tr.relative_seconds = -10;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "relative_neg"), "relative_seconds < 0 -> V7");

    /* An unset type is never armable. */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_INVALID;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_is_valid(&tr, "invalid"), "TYPE_INVALID rejected");
}

/* --- create()/edit() must reject an invalid configuration --- */
TEST_CASE("create and edit reject invalid config", "[esp_schedule]")
{
    /* One rejected shape per rule. */
    esp_schedule_config_t cfg;
    const struct {
        const char *rule;
        esp_schedule_type_t type;
        uint8_t dow, dom;
        uint16_t mon, yr;
        bool ry;
    } bad[] = {
        { "V1",  ESP_SCHEDULE_TYPE_DAYS_OF_WEEK, ESP_SCHEDULE_DAY_MONDAY, 15, 0, 0, false },
        { "V2", ESP_SCHEDULE_TYPE_DATE, ESP_SCHEDULE_DAY_MONDAY, 0, 0, 0, false },
        { "V3", ESP_SCHEDULE_TYPE_DATE, 0, 0, ESP_SCHEDULE_MONTH_ALL, 0, false },
        { "V4", ESP_SCHEDULE_TYPE_DATE, 0, 0, 0, 2026, false },
        { "V5", ESP_SCHEDULE_TYPE_DATE, 0, 15, ESP_SCHEDULE_MONTH_ALL, 2026, true },
        { "V6", ESP_SCHEDULE_TYPE_DATE, 0, 15, 0, 0, true },
    };

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        memset(&cfg, 0, sizeof(cfg));
        strlcpy(cfg.name, "bad_cfg", sizeof(cfg.name));
        cfg.trigger.type = bad[i].type;
        cfg.trigger.hours = 9;
        cfg.trigger.day.repeat_days = bad[i].dow;
        cfg.trigger.date.day = bad[i].dom;
        cfg.trigger.date.repeat_months = bad[i].mon;
        cfg.trigger.date.year = bad[i].yr;
        cfg.trigger.date.repeat_every_year = bad[i].ry;
        char msg[64];
        snprintf(msg, sizeof(msg), "rule %s: create() must return NULL", bad[i].rule);
        TEST_ASSERT_NULL_MESSAGE(esp_schedule_create(&cfg), msg);
    }

    /* V7: RELATIVE with a non-positive delay. Not in the table above because it
     * is the one rule that does not involve a date field. */
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.name, "bad_cfg", sizeof(cfg.name));
    cfg.trigger.type = ESP_SCHEDULE_TYPE_RELATIVE;
    cfg.trigger.relative_seconds = 0;
    TEST_ASSERT_NULL_MESSAGE(esp_schedule_create(&cfg), "rule V7: create() must return NULL");

    /* A valid schedule can be created, and editing it into an invalid shape is
     * rejected without disturbing the stored config. */
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.name, "good_cfg", sizeof(cfg.name));
    cfg.trigger.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    cfg.trigger.hours = 9;
    cfg.trigger.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    esp_schedule_handle_t handle = esp_schedule_create(&cfg);
    TEST_ASSERT_NOT_NULL_MESSAGE(handle, "DOW-1 must be accepted");

    cfg.trigger.date.day = 15; /* rule V1: a date field on a DAYS_OF_WEEK trigger */
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, esp_schedule_edit(handle, &cfg), "edit() must reject rule V1");

    esp_schedule_config_t got;
    memset(&got, 0, sizeof(got));
    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_get(handle, &got));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_SCHEDULE_DAY_MONDAY, got.trigger.day.repeat_days, "rejected edit must not apply");

    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_delete(handle));
}

/* --- Month-mask regression guard (docs/trigger_rules.md §3) ---
 * DATE-2 and DATE-5 differ only in repeat_months: DATE-2 fires once in its year,
 * DATE-5 fires once per masked month of that year. */
TEST_CASE("date one shot vs recurring month mask", "[esp_schedule]")
{
    time_t now = make_time_local(2026, 1, 1, 0, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_local(2028, 1, 1, 0, 0, 0) };
    time_t fired = now - 1;

    /* DATE-2: day = 15, year = 2026, no mask -> one-shot. */
    esp_schedule_trigger_t one_shot;
    memset(&one_shot, 0, sizeof(one_shot));
    one_shot.type = ESP_SCHEDULE_TYPE_DATE;
    one_shot.date.day = 15;
    one_shot.date.year = 2026;
    one_shot.next_scheduled_time_utc = fired;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&one_shot, "DATE-2"), "DATE-2 is valid");
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_fired_and_done(&one_shot, now), "DATE-2 fires once");

    /* DATE-5: the same, plus a full month mask -> one fire per month of 2026. */
    esp_schedule_trigger_t recurring = one_shot;
    recurring.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_trigger_is_valid(&recurring, "DATE-5"), "DATE-5 is valid");
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&recurring, now), "DATE-5 re-arms");

    /* Both arms select the same first instant; only the lifetime differs. */
    time_t a = 0, b = 0;
    TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, 0, 2026, &validity, &a));
    TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, ESP_SCHEDULE_MONTH_ALL, 2026, &validity, &b));
    assert_time_eq("DATE-2 first fire", a, make_time_local(2026, 1, 15, 7, 0, 0));
    assert_time_eq("DATE-5 first fire == DATE-2 first fire", b, a);

    /* DATE-5 walks all 12 masked months of the bound year, then stops. */
    time_t t = now;
    for (int month = 1; month <= 12; month++) {
        time_t next = 0;
        char msg[64];
        snprintf(msg, sizeof(msg), "DATE-5 fire %d of 12", month);
        TEST_ASSERT_TRUE_MESSAGE(esp_schedule_get_next_date_time(t, 7 * 60, 0, 15, ESP_SCHEDULE_MONTH_ALL, 2026, &validity, &next), msg);
        assert_time_eq(msg, next, make_time_local(2026, month, 15, 7, 0, 0));
        t = next;
    }
    time_t none = 0;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_get_next_date_time(t, 7 * 60, 0, 15, ESP_SCHEDULE_MONTH_ALL, 2026, &validity, &none),
                              "DATE-5 expires after the last masked month of its year");
}

/* --- Which year the date arm is bound to, and what that means for the sequence.
 * Three spellings of "recurs over the month set", with three different ends. --- */
TEST_CASE("date months mask year scoping", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 5, 1, 0, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_local(2028, 1, 1, 0, 0, 0) };
    uint16_t months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;

    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    tr.date.repeat_months = months;

    /* DATE-3 - neither year nor repeat_every_year: bound to the current year. */
    TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "DATE-3"));
    TEST_ASSERT_EQUAL_MESSAGE(2025, esp_schedule_date_arm_match_year(&tr, now), "DATE-3 -> current year");
    /* Nothing is stored, so the bound is re-resolved from `now` on every arm. */
    TEST_ASSERT_EQUAL_MESSAGE(2027, esp_schedule_date_arm_match_year(&tr, make_time_local(2027, 3, 1, 0, 0, 0)),
                              "DATE-3 re-resolves the bound from now");

    /* It is therefore not a one-shot: it fires each masked month of that year. */
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-3 re-arms");

    time_t t1 = 0, t2 = 0, t3 = 0;
    uint16_t bound = esp_schedule_date_arm_match_year(&tr, now);
    TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(now, 7 * 60, 0, 15, months, bound, &validity, &t1));
    assert_time_eq("DATE-3 first fire", t1, make_time_local(2025, 6, 15, 7, 0, 0));
    TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(t1, 7 * 60, 0, 15, months, bound, &validity, &t2));
    assert_time_eq("DATE-3 second fire", t2, make_time_local(2025, 7, 15, 7, 0, 0));
    /* Past the last masked month of the bound year there is no match, which the
     * arm path turns into disarm-and-delete. */
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_get_next_date_time(t2, 7 * 60, 0, 15, months, bound, &validity, &t3),
                              "DATE-3 expires after the current year");

    /* DATE-4 - repeat_every_year: unconstrained, so the pattern wraps years. */
    tr.date.repeat_every_year = true;
    TEST_ASSERT_EQUAL_MESSAGE(0, esp_schedule_date_arm_match_year(&tr, now), "DATE-4 -> unconstrained");
    time_t t4 = 0;
    TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(t2, 7 * 60, 0, 15, months,
                     esp_schedule_date_arm_match_year(&tr, t2), &validity, &t4));
    assert_time_eq("DATE-4 wraps into the next year", t4, make_time_local(2026, 6, 15, 7, 0, 0));

    /* DATE-5 - an explicit year: that year regardless of `now`, and the bound
     * survives a reboot because it is stored. */
    tr.date.repeat_every_year = false;
    tr.date.year = 2026;
    TEST_ASSERT_EQUAL_MESSAGE(2026, esp_schedule_date_arm_match_year(&tr, now), "DATE-5 -> its own year");
    TEST_ASSERT_EQUAL_MESSAGE(2026, esp_schedule_date_arm_match_year(&tr, make_time_local(2027, 3, 1, 0, 0, 0)),
                              "DATE-5 bound is independent of now");

    /* No months mask: nothing to iterate, so the year is left unconstrained and
     * the single fire may land in the next year (DATE-1). */
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    TEST_ASSERT_EQUAL_MESSAGE(0, esp_schedule_date_arm_match_year(&tr, now), "DATE-1 -> unconstrained");
}

/* --- DATE-4: unbounded recurrence over the month set (monthly and seasonal) --- */
TEST_CASE("date months mask recurs over month set", "[esp_schedule]")
{
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = make_time_local(2028, 1, 1, 0, 0, 0) };
    time_t now = make_time_local(2025, 1, 1, 0, 0, 0);

    /* Mon = MONTH_ALL: the 15th of every month, wrapping into the next year. */
    const int monthly[][2] = { {1, 15}, {2, 15}, {3, 15} };
    time_t t = now;
    for (size_t i = 0; i < sizeof(monthly) / sizeof(monthly[0]); i++) {
        time_t next = 0;
        TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(t, 7 * 60, 0, 15, ESP_SCHEDULE_MONTH_ALL, 0, &validity, &next));
        assert_time_eq("DATE-4 monthly", next, make_time_local(2025, monthly[i][0], monthly[i][1], 7, 0, 0));
        TEST_ASSERT_TRUE_MESSAGE(next > t, "DATE-4 strictly increasing");
        t = next;
    }

    /* Sparse mask: the 15th of Jun/Jul, every year -> wraps to next June. */
    uint16_t months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;
    const time_t want[] = {
        make_time_local(2025, 6, 15, 7, 0, 0),
        make_time_local(2025, 7, 15, 7, 0, 0),
        make_time_local(2026, 6, 15, 7, 0, 0),
    };
    t = now;
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++) {
        time_t next = 0;
        TEST_ASSERT_TRUE(esp_schedule_get_next_date_time(t, 7 * 60, 0, 15, months, 0, &validity, &next));
        assert_time_eq("DATE-4 seasonal", next, want[i]);
        t = next;
    }

    /* Both spellings are valid triggers that never finish. */
    esp_schedule_trigger_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.type = ESP_SCHEDULE_TYPE_DATE;
    tr.date.day = 15;
    tr.date.repeat_months = months;
    tr.date.repeat_every_year = true;
    tr.next_scheduled_time_utc = now - 1;
    TEST_ASSERT_TRUE(esp_schedule_trigger_is_valid(&tr, "DATE-4 sparse"));
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_trigger_fired_and_done(&tr, now), "DATE-4 never done");
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

    /* year=0 leaves the year unconstrained -> the next occurrence. */
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 10, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "year=0 -> matches next occurrence");
    assert_time_eq("year=0 day=10 09:00", next_ts, make_time_local(2026, 3, 10, 9, 0, 0));

    /* Day-of-month + a FUTURE specific year: the first match is in that year, and
     * once the year is past there is no match at all. This is the engine bound
     * behind DATE-2 (one-shot in that year) and DATE-5 (per masked month, then
     * dead). */
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

    /* Engine-internal: wildcard day + a months mask -> every day in that month.
     * No trigger can reach this (rule V3 rejects a mask without date.day); the
     * rejection is covered by "trigger validation truth table". */
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

/* --- VAL-4: a match that lands before validity.start_time must not be returned;
 * the search continues to the following occurrence (docs §1.2 step 5). This is
 * distinct from VAL-0, where the window opens between two occurrences and the
 * first candidate already qualifies. --- */
TEST_CASE("validity start skips earlier match", "[esp_schedule]")
{
    /* Mondays at 09:00, window opening Mon Jan 20 2025 at 12:00. Jan 20 09:00 is
     * a matching day but falls before start_time -> Mon Jan 27 09:00. */
    time_t now = make_time_local(2025, 1, 15, 8, 0, 0); // Wed
    esp_schedule_validity_t validity = {
        .start_time = make_time_local(2025, 1, 20, 12, 0, 0),
        .end_time = make_time_local(2025, 3, 1, 0, 0, 0),
    };

    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 9 * 60, ESP_SCHEDULE_DAY_MONDAY, 0, 0, 0, &validity, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "VAL-4: match before start_time -> keep searching");
    assert_time_eq("VAL-4 -> Mon Jan 27 09:00", next_ts, make_time_local(2025, 1, 27, 9, 0, 0));

    /* Same shape for the date arm: the 15th, window opening Feb 15 at 12:00. */
    esp_schedule_validity_t val_date = {
        .start_time = make_time_local(2025, 2, 15, 12, 0, 0),
        .end_time = make_time_local(2025, 6, 1, 0, 0, 0),
    };
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 15, 0, 0, &val_date, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "VAL-4 date arm: 15th before start_time -> keep searching");
    assert_time_eq("VAL-4 date arm -> Mar 15 09:00", next_ts, make_time_local(2025, 3, 15, 9, 0, 0));

    /* VAL-0 for contrast: a window opening between two occurrences returns the
     * first candidate at or after start_time, not the one after it. */
    esp_schedule_validity_t val_between = {
        .start_time = make_time_local(2025, 1, 20, 8, 0, 0),
        .end_time = make_time_local(2025, 3, 1, 0, 0, 0),
    };
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, ESP_SCHEDULE_DAY_MONDAY, 0, 0, 0, &val_between, &next_ts);
    TEST_ASSERT_TRUE_MESSAGE(ok, "VAL-0: window opens before the occurrence");
    assert_time_eq("VAL-0 -> Mon Jan 20 09:00", next_ts, make_time_local(2025, 1, 20, 9, 0, 0));
}

/* --- Day/month combinations with no finite match must fail, not loop forever
 * (docs §4.2: "Patterns with no finite match ... exhaust the search horizon and
 * never fire"). --- */
TEST_CASE("date no finite match", "[esp_schedule]")
{
    time_t now = make_time_local(2025, 1, 1, 0, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = 0 }; /* open-ended */

    /* Feb never has a 30th. */
    time_t next_ts = 0;
    bool ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 30, ESP_SCHEDULE_MONTH_FEBRUARY, 0, &validity, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "day=30 masked to February -> no match");
    TEST_ASSERT_TRUE(next_ts == 0);

    /* Apr and Jun are both 30-day months, so a 31st never lands. */
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 31, ESP_SCHEDULE_MONTH_APRIL | ESP_SCHEDULE_MONTH_JUNE, 0, &validity, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "day=31 masked to 30-day months -> no match");
    TEST_ASSERT_TRUE(next_ts == 0);

    /* Feb 29 bounded to a non-leap year cannot be satisfied either. */
    next_ts = 0;
    ok = esp_schedule_get_next_date_time(now, 9 * 60, 0, 29, ESP_SCHEDULE_MONTH_FEBRUARY, 2027, &validity, &next_ts);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Feb 29 in a non-leap year -> no match");
    TEST_ASSERT_TRUE(next_ts == 0);
}

/* --- Time-of-day range validation (docs §4.7, last paragraph): hours < 24 and
 * minutes < 60. Out of range must be refused, not normalized by mktime() into a
 * following day that violates the matched day/month mask. --- */
TEST_CASE("time of day range validation", "[esp_schedule]")
{
    /* Engine level: minutes_since_midnight must stay inside the day. */
    time_t now = make_time_local(2025, 1, 16, 12, 0, 0);
    esp_schedule_validity_t validity = { .start_time = 0, .end_time = 0 };
    time_t next_ts = 0;

    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_get_next_date_time(now, 24 * 60, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts),
                              "minutes_since_midnight == 24*60 -> rejected");
    TEST_ASSERT_TRUE(next_ts == 0);
    TEST_ASSERT_FALSE_MESSAGE(esp_schedule_get_next_date_time(now, 24 * 60 + 30, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts),
                              "minutes_since_midnight past midnight -> rejected");

    /* The last valid minute of the day is accepted. */
    next_ts = 0;
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_get_next_date_time(now, 23 * 60 + 59, ESP_SCHEDULE_DAY_EVERYDAY, 0, 0, 0, &validity, &next_ts),
                             "23:59 -> accepted");
    assert_time_eq("23:59 today", next_ts, make_time_local(2025, 1, 16, 23, 59, 0));

    /* Public API: create() refuses an out-of-range time of day. */
    esp_schedule_config_t cfg;
    const struct {
        const char *what;
        uint8_t hours, minutes;
    } bad[] = {
        { "hours = 24", 24, 0 },
        { "hours = 250", 250, 0 },
        { "minutes = 60", 9, 60 },
        { "minutes = 200", 9, 200 },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        memset(&cfg, 0, sizeof(cfg));
        strlcpy(cfg.name, "bad_time", sizeof(cfg.name));
        cfg.trigger.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
        cfg.trigger.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
        cfg.trigger.hours = bad[i].hours;
        cfg.trigger.minutes = bad[i].minutes;
        char msg[64];
        snprintf(msg, sizeof(msg), "%s: create() must return NULL", bad[i].what);
        TEST_ASSERT_NULL_MESSAGE(esp_schedule_create(&cfg), msg);
    }

    /* 23:59 is accepted, and editing it out of range is refused without
     * disturbing the stored config. */
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.name, "good_time", sizeof(cfg.name));
    cfg.trigger.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    cfg.trigger.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;
    cfg.trigger.hours = 23;
    cfg.trigger.minutes = 59;
    esp_schedule_handle_t handle = esp_schedule_create(&cfg);
    TEST_ASSERT_NOT_NULL_MESSAGE(handle, "23:59 must be accepted");

    cfg.trigger.minutes = 60;
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, esp_schedule_edit(handle, &cfg), "edit() must reject minutes = 60");

    esp_schedule_config_t got;
    memset(&got, 0, sizeof(got));
    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_get(handle, &got));
    TEST_ASSERT_EQUAL_MESSAGE(59, got.trigger.minutes, "rejected edit must not apply");

    /* RELATIVE does not read hours/minutes, so no range applies to them. */
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.name, "rel_time", sizeof(cfg.name));
    cfg.trigger.type = ESP_SCHEDULE_TYPE_RELATIVE;
    cfg.trigger.relative_seconds = 60;
    cfg.trigger.hours = 99;
    cfg.trigger.minutes = 99;
    esp_schedule_handle_t rel = esp_schedule_create(&cfg);
    TEST_ASSERT_NOT_NULL_MESSAGE(rel, "RELATIVE ignores hours/minutes");

    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_delete(rel));
    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_delete(handle));
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

/* --- A stored config that is invalid under the current rules must be dropped on
 * restore, not armed. Configurations written by older versions can contain
 * combinations that are now rejected. --- */
TEST_CASE("nvs restore drops invalid config", "[esp_schedule]")
{
    esp_netif_init(); /* esp_schedule_init() runs SNTP, which needs the TCP/IP stack */
    /* app_main() already brought up NVS for this test app. Start from an empty
     * schedule namespace so the count assertions below are exact. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_nvs_remove_all());

    /* Store a DATE schedule carrying a weekday mask (rule V2) - accepted by
     * 1.3.3, which silently discarded day.repeat_days. */
    esp_schedule_t *stored = calloc(1, sizeof(esp_schedule_t));
    TEST_ASSERT_NOT_NULL(stored);
    strlcpy(stored->name, "stale_cfg", sizeof(stored->name));
    stored->trigger.type = ESP_SCHEDULE_TYPE_DATE;
    stored->trigger.hours = 9;
    stored->trigger.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY;
    stored->trigger.date.day = 15;
    TEST_ASSERT_FALSE(esp_schedule_trigger_is_valid(&stored->trigger, stored->name));
    TEST_ASSERT_EQUAL(ESP_OK, esp_schedule_nvs_add(stored));
    free(stored);

    uint8_t count = 0xFF;
    esp_schedule_handle_t *handle_list = esp_schedule_init(true, NULL, &count);
    TEST_ASSERT_EQUAL_MESSAGE(0, count, "invalid stored config must not be armed");
    free(handle_list);

    /* It was also removed from NVS, so it cannot come back on the next boot. */
    count = 0xFF;
    handle_list = esp_schedule_nvs_get_all(&count);
    TEST_ASSERT_EQUAL_MESSAGE(0, count, "invalid stored config must be deleted from NVS");
    free(handle_list);
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
