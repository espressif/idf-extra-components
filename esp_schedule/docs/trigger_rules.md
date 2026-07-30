# ESP Schedule — Date Engine & Trigger Rules

This document explains, in detail, how `esp_schedule` decides *when* a trigger
fires. It covers:

1. The core date engine (`esp_schedule_get_next_date_time`).
2. How each public trigger type feeds the engine.
3. The "fired-and-done" one-shot rule that decides whether a trigger repeats.
4. A comprehensive table of scenarios showing exactly what each field
   combination does, including the combinations that are **rejected**.
5. The footguns worth knowing before writing a trigger.

The scenario tables in §4 carry a **Unit test** column naming the case in
`test_app/main/test_esp_schedule.c` that verifies each row; `N/A` marks behavior
that follows from the same engine logic but is not yet covered by a dedicated
test. All cases run on target through `test_app/pytest_esp_schedule.py`.

Every scenario row also carries a stable **ID** for quick reference in reviews,
bug reports, and test names:

| ID prefix | Section | Covers                                                  |
|-----------|---------|---------------------------------------------------------|
| `DOW-n`   | §4.1    | `TYPE_DAYS_OF_WEEK` scenarios                           |
| `DATE-n`  | §4.2    | date-arm scenarios — `TYPE_DATE`, and solar's date arm   |
| `SOL-n`   | §4.3    | solar-specific scenarios (`TYPE_SUNRISE` / `TYPE_SUNSET`)|
| `REC-n`   | §4.4    | Common recipes                                          |
| `REL-n`   | §4.5    | `TYPE_RELATIVE` scenarios                               |
| `VAL-n`   | §4.6    | Validity-window scenarios (all types)                   |
| `V1`..`V7`| §4.7    | Rejected configurations (validation rules)              |

---

## 1. The core date engine

Every non-relative trigger ultimately calls one function:

```c
bool esp_schedule_get_next_date_time(time_t now,
                                     uint16_t minutes_since_midnight,
                                     uint8_t  days_of_week_mask,   // Mon=bit0 .. Sun=bit6
                                     uint8_t  day_of_month,        // 1..31
                                     uint16_t months_of_year_mask, // Jan=bit0 .. Dec=bit11
                                     uint16_t year,                // 4-digit, e.g. 2026
                                     const esp_schedule_validity_t *validity,
                                     time_t *next_time);
```

It returns the **next instant at or after `now`** whose local wall-clock date
matches every constraint, at the requested time of day. Each of the four date
fields uses `0` as a wildcard meaning **"any"**.

### 1.1 Field wildcard semantics

| Field                 | `0` means         | non-zero means                          |
|-----------------------|-------------------|-----------------------------------------|
| `days_of_week_mask`   | any weekday       | day-of-week must be in the mask         |
| `day_of_month`        | any day           | `tm_mday` must equal this value          |
| `months_of_year_mask` | any month         | month must be in the mask               |
| `year`                | any year          | bounded to exactly this year            |

The engine matches a day if *either* day arm matches (`days_of_week_mask` OR
`day_of_month`). No trigger can reach that union: validation (§4.7, rule `V2`)
rejects a configuration that sets both arms, so exactly one of the two is ever
non-zero. The union is engine-internal and is covered only by engine-level tests.

### 1.2 What it does (broad steps)

1. **Anchor the search.** Start from `now`; if the validity window opens in the
   future, jump the origin to `start_time` instead.
2. **Skip today if the time already passed.** If today's `HH:MM` is behind us,
   the earliest candidate is tomorrow (unless a future window forces today).
3. **Honor the year.** A year already in the past → no match. A future year →
   jump forward to Jan 1 of that year.
4. **Walk forward, month by month then day by day.** Skip whole months not in
   the months mask; within a month, test each day against the day-match rule
   (§1.1).
5. **First matching day wins.** Build the exact instant at `HH:MM`
   (DST-resolved), check it against the year bound and the validity window, and
   return it. If it falls before the window start, keep searching.
6. **Give up** if nothing matches within the search horizon.
   Months outside the mask do not count against it, so the horizon covers the
   worst satisfiable case, `Feb 29` across the 8-year leap gap (e.g., 2096 -> 2104).
   That worst case is asserted by `date feb29 non leap` (64-bit `time_t` only).

### 1.3 Why it is built this way (implementation notes)

- **Day advance uses `tm_mday++` + `mktime`, never `+86400s`.** Adding a fixed
  86400 seconds across a DST transition skips or repeats a day. Re-normalizing
  the broken-down time is DST-safe.
- **`tm_isdst = -1` before every `mktime`.** This lets libc resolve the correct
  UTC instant for a local wall-clock time. No manual `±3600` correction is
  applied (doing so would double-correct).
- **Validity-start jump (§1.2 step 1).** Without it, a far-future `start_time`
  would exhaust the month-attempt budget before ever reaching the window, and the
  schedule would silently never fire.
- **Day-31 / Feb-29 safety.** Because each candidate is normalized by `mktime`,
  a `day_of_month = 31` in a 30-day month never matches a normalized "1st of
  next month"; it lands on the next month that actually *has* a 31st. `Feb 29`
  correctly skips non-leap years to the next leap year.

### 1.4 DST wall-clock behavior

Schedules fire on **local wall-clock** time (compliant with [RainMaker specification](https://legacy.rainmaker.espressif.com/docs/scheduling/#managing-daylight-saving-time-dst)):

- **Spring forward:** a local time that does not exist (e.g. `02:30` when clocks
  jump `02:00 -> 03:00`) is delayed and fires at `03:30` on the switch day — it
  does **not** skip to the next day.
- **Fall back:** a local time that occurs twice (e.g. `01:30`) fires **once**
  (the first occurrence) and then advances a full day; it does not fire again at
  the repeated hour.

---

## 2. How each trigger type feeds the engine

**Each type reads only the fields it owns.** A field the type does not read is
not silently ignored — the configuration is rejected (§4.7). There is no encoding
for "field not set", so an all-zero date arm is the *wildcard*, not "absent".

| Trigger type            | Time of day             | Day selection                                   | Ignored          |
|-------------------------|-------------------------|-------------------------------------------------|------------------|
| `DAYS_OF_WEEK`          | `hours`:`minutes`       | `day.repeat_days`                                | `solar.*`        |
| `DATE`                  | `hours`:`minutes`       | `date.*`                                         | `solar.*`        |
| `SUNRISE` / `SUNSET`    | solar event + `solar.offset_minutes` | `day.repeat_days` **xor** `date.*` (§2.1) | `hours`, `minutes` |
| `RELATIVE`              | `relative_seconds` from the base time | — (engine not used)                | all date fields, `solar.*` |

Values passed to the engine:

| Trigger type            | `dow_mask`         | `day_of_month`   | `months_mask`         | `year`         |
|-------------------------|--------------------|------------------|-----------------------|----------------|
| `DAYS_OF_WEEK`          | `day.repeat_days`  | `0`              | `0`                   | `0`            |
| `DATE`                  | `0`                | `date.day`       | `date.repeat_months`  | `date.year`    |
| `SUNRISE` / `SUNSET`    | `day.repeat_days`  | `date.day`       | `date.repeat_months`  | `date.year`    |

`date.year` is passed straight through: rule `V5` guarantees it is already `0`
whenever `date.repeat_every_year` is set, so `repeat_every_year` never needs to
mask it out.

### 2.1 Solar picks one day arm

`SUNRISE` / `SUNSET` select their day arm from whichever is populated:

| `day.repeat_days` | any of `date.day` / `repeat_months` / `year` / `repeat_every_year` | Arm         | Outcome                    |
|-------------------|---------------------------------------------------------------------|-------------|----------------------------|
| `> 0`             | none                                                                | day-of-week | `SOL-0` — forever          |
| `0`               | some                                                                | date        | the `DATE-n` rows of §4.2  |
| `> 0`             | some                                                                | —           | **rejected** (rule `V2`)   |
| `0`               | none                                                                | date, all wildcard | `DATE-0`            |

The two arms are never unioned and never intersected. "Weekdays in June" is not
expressible; the nearest option is two separate schedules, or a wider `dow`
schedule bounded by `validity`.

> ### Solar = a date/weekday pattern with a solar time-of-day
>
> Apart from the day-of-week arm above, solar day selection and lifetime rules are
> **identical to `DATE`**. The only difference is the time of day: instead of
> `hours:minutes`, the fire instant is the computed sunrise/sunset for the
> selected day, shifted by `solar.offset_minutes`. Every `DATE-n` row in §4.2
> therefore applies to solar with "at `HH:MM`" replaced by "at sunrise/sunset ±
> offset".

> ### `DATE` has no weekday arm
>
> `ESP_SCHEDULE_TYPE_DATE` reads `date.*` only. Setting `day.repeat_days` on a
> `DATE` trigger is a configuration error (rule `V2`), not an additional filter.
> For a weekday pattern use `ESP_SCHEDULE_TYPE_DAYS_OF_WEEK`; for a weekday
> pattern at sunrise/sunset use a solar trigger with `day.repeat_days` and an
> empty date arm (`SOL-0`).

---

## 3. The one-shot "fired-and-done" rule

After a trigger fires (its `next_scheduled_time_utc` is in the past), the code
asks `esp_schedule_trigger_fired_and_done()` whether to recompute a future
occurrence or leave it dead. This is what separates "fire once" from "repeat".

| Type            | Repeats when…                                                                    |
|-----------------|----------------------------------------------------------------------------------|
| `RELATIVE`      | never — fires exactly once                                                        |
| `DAYS_OF_WEEK`  | `repeat_days != ESP_SCHEDULE_DAY_ONCE` (i.e. any weekday bit set)                  |
| `SUNRISE`, `SUNSET` | `day.repeat_days != 0` (the day-of-week arm), else the date-arm rule below     |
| `DATE`, solar date arm | `repeat_months != 0` — how far it recurs then depends on `year` / `repeat_every_year` (§3.1) |

> ### `repeat_months` gates all date-arm recurrence
>
> A recurrence must name what it recurs *over*, and for the date arm that is the
> month set. With `repeat_months == 0` there is nothing to iterate and the trigger
> fires exactly once, whatever `year` says. This is why:
>
> - `repeat_every_year` with **no** month mask is rejected outright (rule `V6`) —
>   it would be inert. Write it as `repeat_months = ESP_SCHEDULE_MONTH_ALL`.
> - `year` with no month mask is **valid but one-shot** (`DATE-2`): unlike
>   `repeat_every_year`, `year` still constrains *when* the single fire happens.
> - a non-zero `repeat_months` always recurs, including with neither `year` nor
>   `repeat_every_year` (`DATE-3`).

### 3.1 How far a masked date arm recurs — the year constraint

`repeat_months` says *that* the arm recurs; `year` / `repeat_every_year` say *how
far*. The three spellings map to the `year` the arm passes to the engine:

| `year` | `rep_yr` | Year passed to the engine | Ends                                | Shape    |
|--------|----------|---------------------------|-------------------------------------|----------|
| `0`    | `true`   | `0` — unconstrained       | never                               | `DATE-4` |
| `N`    | `false`  | `N`                       | after the last masked month of `N`  | `DATE-5` |
| `0`    | `false`  | **the current year**      | after the last masked month of the current year | `DATE-3` |

In every case the end is enforced by the engine, not by a separate expiry check:
once the bound year is exhausted there is no further match, and the arm path
disarms and deletes the schedule.

> **`DATE-3`'s bound is not persistent.** With `year = 0` nothing is stored, so
> the bound is re-resolved from the current time on every arm. A device that
> reboots in a later calendar year re-bounds the schedule to *that* year and fires
> it again. Use `year = N` when the end date must survive a reboot, or
> `repeat_every_year` when it should never end.

---

## 4. Comprehensive scenario tables

`HH:MM` = the configured time of day. "Now" is arbitrary unless stated.
`dow` = `day.repeat_days`, `day` = `date.day`, `months` = `date.repeat_months`,
`year`/`rep_yr` = `date.year` / `date.repeat_every_year`.

### 4.1 `ESP_SCHEDULE_TYPE_DAYS_OF_WEEK`

Only `repeat_days` is read. Any `date.*` field set is rejected (rule `V1`).

| ID      | `repeat_days`            | Lifetime | Behavior                                    | Unit test |
|---------|--------------------------|----------|---------------------------------------------|-----------|
| `DOW-0` | `DAY_ONCE` (`0`)         | one-shot | Fires **once** at the next `HH:MM` (today if not yet passed, else tomorrow), then done. | `one-shot fired and done`, `trigger validation truth table` |
| `DOW-1` | `THURSDAY`               | forever  | Every Thursday at `HH:MM`.                  | `day of week`, `knife edge now equals target` |
| `DOW-2` | `MONDAY \| WEDNESDAY`    | forever  | Every Mon **and** Wed at `HH:MM`.           | `sequence dow mon wed` |
| `DOW-3` | `DAY_EVERYDAY` (`0x7F`)  | forever  | Daily at `HH:MM`. **This is the daily recipe.** | `dst daily across spring forward` |
| `DOW-4` | any mask, `HH:MM` = non-existent local time (spring forward) | forever | Delayed within the switch day (`02:30` → `03:30`); no day skipped (§1.4). | `dst rainmaker spring forward delayed one hour`, `dst skipped local time` |
| `DOW-5` | any mask, `HH:MM` = repeated local time (fall back) | forever | Fires **once** (first occurrence), then advances a full day (§1.4). | `dst rainmaker fall back fires once`, `dst daily across fall back` |
| `DOW-6` | mask near midnight, next-day rollover | forever | Next matching weekday resolved correctly across a DST-affected midnight. | `dst next dow near midnight` |

### 4.2 The date arm — `ESP_SCHEDULE_TYPE_DATE`, and solar's date arm

`dow` is `0` throughout (a weekday mask here is rejected by `V2`). These six
shapes are the **complete** set of accepted date-arm configurations; every other
combination of the four date fields is rejected (§4.7).

| ID       | `day` | `months`        | `year`/`rep_yr` | Lifetime           | Behavior                                                              | Unit test |
|----------|-------|-----------------|-----------------|--------------------|-----------------------------------------------------------------------|-----------|
| `DATE-0` | `0`   | `0`             | `0`/`false`     | one-shot           | All wildcards → fires **once** at the next `HH:MM`.                    | `one-shot fired and done`, `date wildcard and daily` |
| `DATE-1` | `15`  | `0`             | `0`/`false`     | one-shot           | Fires **once** on the next 15th.                                       | `one-shot fired and done`, `date wildcard and daily` |
| `DATE-2` | `15`  | `0`             | `2026`/`false`  | one-shot           | Fires **once**, on the next 15th **in 2026**. Dropped if 2026 is past.  | `one-shot fired and done`, `date one shot vs recurring month mask` |
| `DATE-3` | `15`  | `Jun\|Jul`      | `0`/`false`     | expires after the current year | 15th of each masked month **of the current year**, then dead. ⚠️ The bound is re-resolved on every arm, so it is not reboot-safe (§3.1). | `date months mask year scoping`, `one-shot fired and done` |
| `DATE-4` | `15`  | `Jun\|Jul`      | `0`/`true`      | forever            | 15th of each masked month, **every year**. With `months = MONTH_ALL`, "the 15th of every month". | `date months mask recurs over month set` |
| `DATE-5` | `15`  | `Jun\|Jul`      | `2026`/`false`  | expires after 2026 | 15th of each masked month **in 2026**, then dead.                       | `date one shot vs recurring month mask`, `date months year bounded expires` |
| `DATE-6` | `31`  | `Apr\|May`      | `0`/`true`      | forever            | Skips 30-day April's non-existent 31st → lands on **May 31**.            | `date permutations more` |
| `DATE-7` | `29`  | `Feb`           | `0`/`true`      | forever            | **Feb 29 skips non-leap years** to the next leap Feb 29 (up to the 8-year gap, §1.2 step 6). | `date feb29 non leap` |
| `DATE-8` | `1`   | `Nov\|Dec\|Jan` | `0`/`true`      | forever            | 1st of Nov/Dec/Jan, across the year boundary.                           | `date permutations more` |

`DATE-6`..`DATE-8` are `DATE-4` with awkward day/month combinations; they are
listed separately because the engine's day-normalization behavior is the point.

`DATE-2` vs `DATE-5` is the pair that shows what `repeat_months` does: identical
`day` and `year`, identical *first* fire, but one fire versus one fire per masked
month. `date one shot vs recurring month mask` is the regression guard for it.
`DATE-3` vs `DATE-4` vs `DATE-5` then differ only in how far that recurrence
reaches (§3.1), which `date months mask year scoping` pins down.

Patterns with no finite match — `day = 30` masked to February, `day = 31` masked
to only 30-day months, `Feb 29` bounded to a non-leap `year` — exhaust the search
horizon and never fire (`date no finite match`).

### 4.3 `ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`

Requires `CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT`. `off` = `solar.offset_minutes`;
the fire instant is the solar event for the selected day, plus `off`.

Solar's date arm is §4.2 verbatim, so only the solar-specific rows are listed
here.

| ID      | `dow`          | date arm | `off` | Lifetime | Behavior                                                                | Unit test |
|---------|----------------|----------|-------|----------|-------------------------------------------------------------------------|-----------|
| `SOL-0` | `Mon..Fri`     | empty    | `0`   | forever  | Solar event on every set weekday, indefinitely. **The weekday recipe.**   | `solar with dow`, `solar variants` |
| `SOL-1` | `DAY_EVERYDAY` | empty    | `0`   | forever  | Solar event **every day**. Selects the same first day as `DATE-0`, but repeats. **The daily recipe.** | `solar variants` |
| `SOL-2` | `EVERYDAY`     | empty    | `-30`/`+15` | forever | Fires 30 min before / 15 min after the solar event.                  | `solar variants` |
| `SOL-3` | any            | any      | any   | as above | Polar night/day: days with **no solar event** are skipped, search advances up to ~370 days; no event in window → no schedule. | N/A (location-dependent) |
| `SOL-4` | any repeating  | any      | any   | forever  | Consecutive fire instants stay strictly increasing as solar times drift day to day. | `solar sequence monotonic` |

Solar-specific notes:
- `offset_minutes` may be negative (before) or positive (after). Solar times are
  computed in UTC; the calendar date is taken from the selected day's local date.
- The day is selected by the same date engine (evaluated at 23:59 so selection is
  date-only), then the actual sunrise/sunset instant for that day is computed.
- `SUNRISE` vs `SUNSET` only changes which event is used; day selection is
  identical.
- A solar **weekday** pattern cannot be year-bounded (rule `V2` — `year` belongs
  to the date arm). Use `validity.end_time` instead.

### 4.4 Common recipes

| ID      | Goal                                          | Type          | Fields                                                        | Shape    |
|---------|-----------------------------------------------|---------------|---------------------------------------------------------------|----------|
| `REC-0` | Once, at the next `HH:MM`                     | `DAYS_OF_WEEK`| `repeat_days = DAY_ONCE`                                       | `DOW-0`  |
| `REC-1` | Daily at `HH:MM`                              | `DAYS_OF_WEEK`| `repeat_days = DAY_EVERYDAY`                                   | `DOW-3`  |
| `REC-2` | Every Mon/Wed/Fri at `HH:MM`                  | `DAYS_OF_WEEK`| `repeat_days = MON\|WED\|FRI`                                   | `DOW-2`  |
| `REC-3` | Once, on the next 15th                        | `DATE`        | `day = 15`                                                     | `DATE-1` |
| `REC-4` | Once, on the next 15th in 2026                | `DATE`        | `day = 15`, `year = 2026`                                      | `DATE-2` |
| `REC-5` | 15th of every month, forever                  | `DATE`        | `day = 15`, `months = MONTH_ALL`, `rep_yr = true`                | `DATE-4` |
| `REC-6` | 15th of Jun/Jul/Aug, every year               | `DATE`        | `day = 15`, `months = Jun\|Jul\|Aug`, `rep_yr = true`            | `DATE-4` |
| `REC-7` | 15th of every month in 2026, then stop        | `DATE`        | `day = 15`, `months = MONTH_ALL`, `year = 2026`                  | `DATE-5` |
| `REC-8` | Once, at the next sunrise                     | `SUNRISE`     | lat/lon/offset only                                             | `DATE-0` |
| `REC-9` | Every day at sunrise                          | `SUNRISE`     | `repeat_days = DAY_EVERYDAY`                                    | `SOL-1`  |
| `REC-10`| Weekdays, 30 min before sunset                | `SUNSET`      | `repeat_days = MON..FRI`, `offset_minutes = -30`                 | `SOL-0`  |
| `REC-11`| Sunset on the 1st of every month              | `SUNSET`      | `day = 1`, `months = MONTH_ALL`, `rep_yr = true`                  | `DATE-4` |
| `REC-12`| Sunrise on the 15th of Jun/Jul 2026, then stop| `SUNRISE`     | `day = 15`, `months = Jun\|Jul`, `year = 2026`                    | `DATE-5` |
| `REC-13`| Once, N seconds from now                      | `RELATIVE`    | `relative_seconds = N`                                          | `REL-0`  |

Not expressible by design, with the nearest alternative:

| Not expressible                                        | Reason | Nearest                                    |
|--------------------------------------------------------|--------|--------------------------------------------|
| Every day of a month range                             | `V3`   | `DAYS_OF_WEEK` + `EVERYDAY` (not month-scoped) |
| Weekday **union** a date pattern                       | `V2`   | two separate schedules                     |
| Weekday **intersected** with a date pattern ("weekdays in June") | `V2` | none                              |
| Masked months recurring with neither `year` nor `rep_yr`| `DATE-3` is one-shot | `DATE-4` (forever) or `DATE-5` (bounded) |
| `rep_yr` without a month mask                          | `V6`   | add `months = MONTH_ALL` (`DATE-4`)        |
| A solar weekday pattern bounded to one year            | `V2`   | `validity.end_time`                        |

### 4.5 `ESP_SCHEDULE_TYPE_RELATIVE`

`relative_seconds` from the base time (`now`, or `validity.start_time` if that is
in the future). Fires exactly **once**. Not recomputed after firing. Date fields
are ignored — the engine is not used, and no date field can invalidate the
configuration. `relative_seconds` itself must be **> 0** (rule `V7`).

| ID      | `relative_seconds` | `validity.start_time` | Lifetime | Behavior                                                    | Unit test |
|---------|--------------------|-----------------------|----------|-------------------------------------------------------------|-----------|
| `REL-0` | `N`                | `0` / past            | one-shot | Fires once at `now + N`, then done.                          | `one-shot fired and done` |
| `REL-1` | `N`                | future                | one-shot | Base jumps to `start_time`; fires once at `start_time + N`.   | N/A |
| `REL-2` | `N`                | `end_time < base + N` | expired  | Occurrence past `end_time` → no match, schedule expires (`VAL-1`). | N/A |
| `REL-3` | `0` or negative    | any                   | —        | **Rejected** (`V7`): the target would be at or before the base time, so there is nothing to fire. | `trigger validation truth table`, `create and edit reject invalid config` |

### 4.6 Validity window (`esp_schedule_validity_t`) — applies to all types

| ID      | Situation                                   | Behavior                                                        | Unit test |
|---------|---------------------------------------------|----------------------------------------------------------------|-----------|
| `VAL-0` | `start_time` in the future                  | Search origin jumps to `start_time`; result is `>= start_time`. | `validity respected`, `validity start skips earlier match` |
| `VAL-1` | Next occurrence would be after `end_time`   | Returns no match → schedule expires.                           | `sequence validity cutoff` |
| `VAL-2` | Callback dispatch delayed past `end_time`   | Trigger is suppressed at fire time (re-check in the timer callback). | N/A (runtime timer path, not unit-testable without a live timer) |
| `VAL-3` | `start_time == 0` / `end_time == 0` (or NULL) | That bound is disabled (open-ended).                        | `validity respected` |
| `VAL-4` | First match falls before `start_time`       | Search continues past it instead of returning it (§1.2 step 5). | `validity start skips earlier match` |

### 4.7 Rejected configurations

A configuration that sets a field its type does not read, or that combines fields
with no coherent reading, is **rejected** rather than partly ignored.
`esp_schedule_create()` returns `NULL`, `esp_schedule_edit()` returns
`ESP_ERR_INVALID_ARG`, and a configuration restored from NVS is deleted instead
of armed. The offending field is logged.

Rules are evaluated in order; the first match is the reported cause.

| Rule | Condition                                                                          | Applies to | Why |
|------|------------------------------------------------------------------------------------|------------|-----|
| `V1` | any of `date.day`, `date.repeat_months`, `date.year`, `date.repeat_every_year` set  | `DAYS_OF_WEEK` | The type reads only `day.repeat_days`. |
| `V2` | `day.repeat_days != 0`                                                             | `DATE` always; solar only when the date arm is also populated | The weekday and date arms are exclusive (§2.1). |
| `V3` | `date.day == 0` and `repeat_months != 0`                                            | date arm | Would mean *every day* of the masked months — never an intended schedule. |
| `V4` | `date.day == 0` and `repeat_months == 0` and (`year != 0` or `repeat_every_year`)    | date arm | A recurrence or a year bound with no date pattern to apply it to. |
| `V5` | `year != 0` and `repeat_every_year`                                                 | date arm | Contradictory intent: "only year N" vs "every year". |
| `V6` | `repeat_every_year` and `repeat_months == 0`                                         | date arm | `repeat_every_year` recurs over the month set (§3); with no mask it would be inert. |
| `V7` | `relative_seconds <= 0`                                                             | `RELATIVE` | The target would be at or before the base time; the schedule could never fire. |

Counting over `dow × day × months × year × rep_yr` with each field reduced to
zero / non-zero — 32 combinations per type:

| Type                 | Accepted        | Repeating, unbounded | Repeating, year-bounded |
|----------------------|-----------------|----------------------|-------------------------|
| `DAYS_OF_WEEK`       | 2 (`DOW-0`, `DOW-1`) | `DOW-1`          | —                       |
| `DATE`               | 6 (`DATE-0`..`DATE-5`) | `DATE-4`       | `DATE-3` (current year), `DATE-5` (named year) |
| `SUNRISE` / `SUNSET` | 7 (`DATE-0`..`DATE-5`, `SOL-0`) | `DATE-4`, `SOL-0` | `DATE-3`, `DATE-5` |
| `RELATIVE`           | all 32 (no date field is read; `relative_seconds > 0` is the only requirement) | — | — |

`DAYS_OF_WEEK` reaches only 2 because `V1` rejects all 30 combinations with any
date field set. The full 32-combination table for every type is asserted by
`trigger validation truth table`, and `create and edit reject invalid config`
covers one rejected shape per rule through the public API.

Range validation is separate and unchanged: `hours < 24 && minutes < 60`, checked
by `create()` / `edit()` and by the engine itself
(`minutes_since_midnight < 24*60`); `RELATIVE` reads neither field, so no range
applies to it. `time of day range validation` covers all three paths. No range
check is applied to `latitude` / `longitude` / `offset_minutes`; a zeroed-out
solar config computes sunrise/sunset for 0°N 0°E, which is a valid location.

---

## 5. Footguns

1. **`repeat_months` is what makes a date arm recur.** `day = 15` alone
   (`DATE-1`) and `day = 15` with a `year` (`DATE-2`) fire exactly **once** — the
   `year` narrows the single fire, it does not repeat it. For "the 15th of every
   month" set `months = MONTH_ALL` *and* `rep_yr = true` (`DATE-4`).
2. **A months mask with no `year` and no `rep_yr` stops at the end of the current
   year** (`DATE-3`), and because nothing is stored, a reboot in the next calendar
   year starts it again for that year (§3.1). Say what you mean: `rep_yr = true`
   for never-ending, `year = N` for a persistent end date.
3. **`repeat_every_year` alone is rejected**, not helpful — it has no month set
   to recur over (`V6`). Add `months = MONTH_ALL`.
4. **`year` and `repeat_every_year` together are rejected** (`V5`). Pick one:
   `year` bounds the schedule to that year, `repeat_every_year` never expires.
5. **A weekday mask on a `DATE` trigger is rejected** (`V2`), and so is a
   weekday mask plus any date field on a solar trigger. Daily is
   `DAYS_OF_WEEK` + `EVERYDAY` (`DOW-3`), or a solar trigger with
   `repeat_days = EVERYDAY` and an empty date arm (`SOL-1`).
6. **A months mask needs a `date.day`** (`V3`). There is no "every day of June"
   date shape; the nearest is `DAYS_OF_WEEK` + `EVERYDAY`, which is not
   month-scoped.
7. **A zero field is a wildcard, not "unset".** There is no encoding for
   "absent", so an all-zero date arm means "any day, any month, any year" — that
   is `DATE-0`, a one-shot at the next `HH:MM`, and a zeroed solar config
   computes sunrise/sunset for 0°N 0°E.
8. **Setting a field the type does not read is an error**, not a no-op: the
   schedule is refused at `create()` / `edit()` and deleted on NVS restore
   (§4.7). Only `RELATIVE` ignores date fields — but its own
   `relative_seconds` must be `> 0` (`V7`).
