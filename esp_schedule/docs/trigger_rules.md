# ESP Schedule — Date Engine & Trigger Rules

This document explains, in detail, how `esp_schedule` decides *when* a trigger
fires. It covers:

1. The core date engine (`esp_schedule_get_next_date_time`).
2. How each public trigger type feeds the engine.
3. The "fired-and-done" one-shot rule that decides whether a trigger repeats.
4. A comprehensive table of scenarios showing exactly what each field
   combination does.
5. A short semantic rule to remember it all.

The scenario tables in §4 carry a **Unit test** column naming the case in
`test_app/main/test_esp_schedule.c` that verifies each row; `N/A` marks behavior
that follows from the same engine logic but is not yet covered by a dedicated
test.

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
| `days_of_week_mask`   | (see day rule)    | day-of-week must be in the mask         |
| `day_of_month`        | (see day rule)    | `tm_mday` must equal this value         |
| `months_of_year_mask` | any month         | month must be in the mask               |
| `year`                | any year          | bounded to exactly this year            |

### 1.2 What it does (broad steps)

1. **Anchor the search.** Start from `now`; if the validity window opens in the
   future, jump the origin to `start_time` instead.
2. **Skip today if the time already passed.** If today's `HH:MM` is behind us,
   the earliest candidate is tomorrow (unless a future window forces today).
3. **Honor the year.** A year already in the past → no match. A future year →
   jump forward to Jan 1 of that year.
4. **Walk forward, month by month then day by day.** Skip whole months not in
   the months mask; within a month, test each day against the day-match rule
   (§1.1 / §4).
5. **First matching day wins.** Build the exact instant at `HH:MM`
   (DST-resolved), check it against the year bound and the validity window, and
   return it. If it falls before the window start, keep searching.
6. **Give up** if nothing matches within the search horizon (~25 months).

### 1.3 Why it is built this way (implementation notes)

- **Day advance uses `tm_mday++` + `mktime`, never `+86400s`.** Adding a fixed
  86400 seconds across a DST transition skips or repeats a day. Re-normalizing
  the broken-down time is DST-safe.
- **`tm_isdst = -1` before every `mktime`.** This lets libc resolve the correct
  UTC instant for a local wall-clock time. No manual `±3600` correction is
  applied (doing so would double-correct).
- **Validity-start jump (§1.2.1).** Without it, a far-future `start_time` would
  exhaust the ~25-month walk before ever reaching the window, and the schedule
  would silently never fire.
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

This is the crucial part: **not every trigger type passes every field to the
engine.** The table shows what each type actually passes.

| Trigger type            | `dow_mask`         | `day_of_month`   | `months_mask`         | `year`\*             |
|-------------------------|--------------------|------------------|-----------------------|----------------------|
| `DAYS_OF_WEEK`          | `day.repeat_days`  | `0`              | `0`                   | `0`                  |
| `DATE`                  | `day.repeat_days`  | `date.day`       | `date.repeat_months`  | see note\*           |
| `SUNRISE` / `SUNSET`    | `day.repeat_days`  | `date.day`       | `date.repeat_months`  | see note\*           |
| `RELATIVE`              | *engine not used — fires `now + relative_seconds` once*                            |

\* **Year for DATE and solar:** if `date.repeat_every_year == true`, `year` is
passed as `0` (pattern recurs yearly); otherwise `date.year` is passed (bounds it
to that one year).

> ### Solar = DATE with a solar time-of-day
>
> `SUNRISE` / `SUNSET` use the **same day selection and repeat rules as `DATE`**
> (day-of-week OR day-of-month, months mask, year, `repeat_every_year`, one-shot).
> The only difference is the time of day: instead of `hours:minutes`, the fire
> instant is the computed sunrise/sunset for the selected day, shifted by
> `solar.offset_minutes`. Everywhere this document describes DATE behavior, solar
> behaves identically with that substitution.

> ### DATE combines day-of-week OR day-of-month
>
> `ESP_SCHEDULE_TYPE_DATE` passes **both** `day.repeat_days` and `date.day` to
> the engine, so a DATE trigger can fire on a weekday pattern, a day-of-month, or
> the **union** of both (the engine ORs them — see the day-match rule in §1.1 /
> §4). `day.repeat_days` on a DATE trigger follows the **same one-shot rule as
> day-of-month**: it fires once on the next matching day unless
> `repeat_every_year` (or a months mask / specific year) keeps it going (§3).
>
> Because the two are OR'd, `ESP_SCHEDULE_DAY_EVERYDAY` (all 7 weekday bits set)
> **subsumes any `date.day`**: `EVERYDAY | day=15` matches *every* day, not the
> 15th. The day-of-month is not a filter here — to fire on the 15th only, leave
> `day.repeat_days = 0`.

---

## 3. The one-shot "fired-and-done" rule

After a trigger fires (its `next_scheduled_time_utc` is in the past), the code
asks `esp_schedule_trigger_fired_and_done()` whether to recompute a future
occurrence or leave it dead. This is what separates "fire once" from "repeat".

| Type            | Considered done (one-shot) when…                                             |
|-----------------|------------------------------------------------------------------------------|
| `RELATIVE`      | always — fires exactly once                                                   |
| `DAYS_OF_WEEK`  | `repeat_days == ESP_SCHEDULE_DAY_ONCE` (i.e. `0`)                             |
| `DATE`, `SUNRISE`, `SUNSET` | `repeat_every_year == false && repeat_months == 0 && year == 0`  |

Key consequences for `DATE` and solar (identical rules):

- `repeat_every_year == true` → **never** done (recurs yearly).
- `repeat_months != 0` → **not** marked done here; the schedule keeps firing
  through the masked months. Whether it ever expires depends on `year`:
  - `year == 0` → the masked-month pattern **repeats every year** (there is no
    stored anchor to expire against, so `repeat_every_year` is redundant here).
  - `year == N` → the **engine** bounds it: it fires that year's masked months
    (e.g. "16th of Jun/Jul/Aug 2026"), then no further match → it expires. This
    is the v1.3.3 `is_expired` behavior, which anchored expiry to `date.year`.
- `year != 0` (day-only) → not marked done here; the engine bounds it and it
  expires after that year.
- Only when **all** of `repeat_every_year`, `repeat_months`, and `year` are
  absent is it a true one-shot (fires on the next matching day, then done — even
  if a day-of-week/day-of-month pattern is set).

> **To make a months-mask schedule expire after its season, set `date.year`.**
> With `year = 0` a months mask runs every year.

---

## 4. Comprehensive scenario tables

`HH:MM` = the configured time of day. "Now" is arbitrary unless stated.

### 4.1 `ESP_SCHEDULE_TYPE_DAYS_OF_WEEK`

Only `repeat_days` matters.

| `repeat_days`            | Behavior                                             | Unit test |
|--------------------------|-----------------------------------------------------|-----------|
| `DAY_ONCE` (`0`)         | Fires **once** at the next `HH:MM` (today if not yet passed, else tomorrow), then done. | `one-shot fired and done` |
| `THURSDAY`               | Every Thursday at `HH:MM`, forever.                  | `knife edge now equals target` |
| `MONDAY \| WEDNESDAY`    | Every Mon **and** Wed.                               | `sequence dow mon wed` |
| `DAY_EVERYDAY` (`0x7F`)  | Daily at `HH:MM`.                                    | `dst daily across spring forward` |

### 4.2 `ESP_SCHEDULE_TYPE_DATE`

`day` = `date.day`, `months` = `date.repeat_months`, `year`/`rep_yr` =
`date.year` / `date.repeat_every_year`. `dow` = `day.repeat_days`, which is now
also honored (OR'd with `day`, like solar — §2).

All four date fields in one table. `dow = 0` means the day-of-week arm is
unused (day selection comes from `day`/`months`, or is a pure wildcard).

| `dow`      | `day` | `months`        | `year`/`rep_yr` | Behavior                                                            | Unit test |
|------------|-------|-----------------|-----------------|--------------------------------------------------------------------|-----------|
| `Mon`      | `0`   | `0`             | `0`/`false`     | Fires **once** on the next Monday, then done (same one-shot rule as day-of-month). | `date type dow one shot` |
| `Mon`      | `0`   | `0`             | `0`/`true`      | Every Monday, forever.                                             | `date type dow one shot` |
| `EVERYDAY` | `0`   | `0`             | `0`/`true`      | Daily, forever — same as `dow = 0`, `rep_yr = true` (mask ignored for repeat). | `date wildcard and daily`, `one-shot fired and done` |
| `EVERYDAY` | `0`   | `0`             | `0`/`false`     | ⚠️ **One-shot** (fires once). Use `rep_yr = true` for a daily DATE schedule. | `one-shot fired and done` |
| `EVERYDAY` | `15`  | `0`             | `0`/`true`      | Every day (EVERYDAY OR'd with the 15th = **all days**; the day-of-month is subsumed, not a filter). ⚠️ Likely a mistake — for "only the 15th" use `dow = 0`. | `date everyday subsumes dom` |
| `Mon`      | `15`  | `0`             | `0`/`true`      | Every Monday **OR** the 15th (union), forever.                    | `date type dow or dom` |
| `Mon`      | `15`  | `Jun`           | `0`/`true`      | In June each year: Mondays OR the 15th (union, months-scoped).     | `date type dow or dom` |
| `Mon`      | `0`   | `Jun`           | `0`/`true`      | Every **Monday in June**, every year (dow + months mask, no dom).  | `date dow months mask` |
| `Mon\|Wed\|Fri` | `0` | `Jun`        | `0`/`true`      | Every **Mon/Wed/Fri in June**, every year (multi-dow + months mask). | `date dow months mask` |
| `0`        | `0`   | `0`             | `0`/`false`     | All wildcards → engine picks next day, but one-shot guard fires it **once** at next `HH:MM`. | `date wildcard and daily`, `one-shot fired and done` |
| `0`        | `0`   | `0`             | `0`/`true`      | Wildcard day, never done → fires **daily**, forever.                     | `date wildcard and daily`, `one-shot fired and done` |
| `0`        | `15`  | `0`             | `0`/`false`     | Fires **once** on the next 15th at `HH:MM`, then done.                    | `date wildcard and daily`, `one-shot fired and done` |
| `0`        | `15`  | `0`             | `0`/`true`      | **15th of every month, forever.**                                        | `one-shot fired and done` |
| `0`        | `20`  | `Jan\|Mar`      | `0`/`false`     | 20th of Jan and Mar, **every year** (year=0 → no expiry).                | `date permutations` |
| `0`        | `15`  | `Jan\|Mar\|Apr` | `0`/`false`     | 15th of Jan, Mar, Apr, **every year** (wraps into the next year).        | `sequence date months mask` |
| `0`        | `15`  | `Jan\|Mar\|Apr` | `0`/`true`      | Same as the row above — `rep_yr` is redundant when `year = 0`.           | `sequence date months mask` |
| `0`        | `15`  | `Jun\|Jul`      | `2025`/`false`  | 15th of Jun & Jul **2025 only**, then expires (v1.3.3 season expiry).    | `date months year bounded expires` |
| `0`        | `5`   | `0`             | `2026`/`false`  | Fires on the 5th of each month **in 2026 only**, then dead.              | `date year bounding` |
| `0`        | `10`  | `0`             | `2025`(past)/`false` | No match → schedule expires immediately.                            | `date year bounding` |
| `0`        | `31`  | `Apr\|May`      | `0`/`false`     | Skips 30-day April's non-existent 31st → lands on **May 31**.            | `date permutations more` |
| `0`        | `29`  | `Feb`           | `0`/`true`      | **Feb 29 skips non-leap years** to the next leap Feb 29.                 | `date feb29 non leap` |
| `0`        | `0`   | `Jun`           | `0`/`true`      | Wildcard day + June filter → fires **every day in June**, every year. ⚠️ Rarely intended. | `date wildcard and daily` |
| `0`        | `1`   | `Nov\|Dec\|Jan` | `0`/`false`     | 1st of Nov/Dec/Jan across the year boundary.                            | `date permutations more` |

### 4.3 `ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`

Requires `CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT`.

**Day selection and repeat/one-shot semantics are identical to `DATE` (§4.2)** —
`day.repeat_days`, `date.day`, `date.repeat_months`, `date.year`, and
`date.repeat_every_year` behave exactly the same. The only difference is the
time of day: instead of `HH:MM`, the fire instant is the computed sunrise/sunset
for the selected day, shifted by `solar.offset_minutes`. So every row in §4.2
applies here with "at `HH:MM`" replaced by "at sunrise/sunset ± offset".

Scenario table (`off` = `solar.offset_minutes`; fire time = solar event + `off`):

| `dow`      | `day` | `months`        | `year`/`rep_yr` | `off` | Behavior                                                          | Unit test |
|------------|-------|-----------------|-----------------|-------|-------------------------------------------------------------------|-----------|
| `0`        | `0`   | `0`             | `0`/`false`     | `0`   | **One-shot**: fires on the next day's solar event, then done.     | `solar one shot matches date` |
| `0`        | `0`   | `0`             | `0`/`true`      | `0`   | **Every day**, forever, at the exact solar event. *(the daily recipe)* | `solar variants` |
| `EVERYDAY` | `0`   | `0`             | `0`/`true`      | `0`   | **Every day**, forever — identical to `dow = 0` above (mask ignored for repeat). | `solar variants` |
| `EVERYDAY` | `0`   | `0`             | `0`/`false`     | `0`   | ⚠️ **One-shot** (fires once). `EVERYDAY` does *not* make it daily; `rep_yr` does. | `solar one shot matches date` |
| `0`        | `0`   | `0`             | `0`/`true`      | `-30` | Every day, **30 min before** the solar event.                     | `solar variants` |
| `0`        | `0`   | `0`             | `0`/`true`      | `+15` | Every day, **15 min after** the solar event.                      | `solar variants` |
| `Mon..Fri` | `0`   | `0`             | `0`/`true`      | `0`   | Solar event every weekday, forever.                               | `solar with dow` |
| `Mon..Fri` | `0`   | `0`             | `0`/`false`     | `0`   | ⚠️ **One-shot** (next weekday only). Add `rep_yr` for a recurring weekday schedule. | `solar one shot matches date` |
| `Sat\|Sun` | `0`   | `0`             | `0`/`true`      | `0`   | Solar event every weekend day, forever.                           | `solar variants` |
| `0`        | `15`  | `0`             | `0`/`true`      | `0`   | Solar event on the 15th of **every month**, forever.              | `solar variants` |
| `0`        | `15`  | `0`             | `0`/`false`     | `0`   | **One-shot**: next 15th only.                                     | `solar one shot matches date` |
| `0`        | `15`  | `Jun\|Jul\|Aug` | `0`/`false`     | `-15` | 15 min before the solar event on the 15th of Jun/Jul/Aug, **every year** (year=0 → no expiry). | `solar with date mask` |
| `0`        | `15`  | `Jun\|Jul`      | `2025`/`false`  | `0`   | 15th of Jun & Jul **2025 only**, then expires (v1.3.3 season expiry). | `date months year bounded expires` (shared engine) |
| `Mon`      | `15`  | `0`             | `0`/`true`      | `0`   | Solar event on **every Monday OR the 15th** (union), forever.     | `solar variants` |
| `0`        | `0`   | `Jun\|Jul\|Aug` | `0`/`true`      | `0`   | ⚠️ Solar event **every day** in Jun/Jul/Aug, every year (day wildcard + months filter). | `solar variants` |
| `Mon`      | `0`   | `0`             | `2026`/`false`  | `0`   | Solar event every Monday **in 2026 only**, then dead (year bounds it). | `solar variants` |

Solar-specific notes:
- Days with **no solar event** (polar night/day) are skipped; the search
  advances up to ~370 days. A location that never sees the event in the window
  yields no schedule.
- `offset_minutes` may be negative (before) or positive (after). Solar times are
  computed in UTC; the calendar date is taken from the selected day's local date.
- The day is selected by the same date engine (evaluated at 23:59 so selection is
  date-only), then the actual sunrise/sunset instant for that day is computed.
- `SUNRISE` vs `SUNSET` only changes which event is used; day selection is
  identical.

#### 4.3.1 Common solar recipes

Same fields as the scenario table above (`type` = `SUNRISE`/`SUNSET`,
`off` = `solar.offset_minutes`). **Bold** marks the field that makes the recipe work.

| Goal                              | `type`    | `dow`      | `day` | `months`        | `year`/`rep_yr` | `off` |
|-----------------------------------|-----------|------------|-------|-----------------|-----------------|-------|
| Every day at sunrise              | `SUNRISE` | `0`        | `0`   | `0`             | `0`/**`true`**  | `0`   |
| Every day, 30 min before sunset   | `SUNSET`  | `0`        | `0`   | `0`             | `0`/**`true`**  | **`-30`** |
| Weekdays only at sunrise          | `SUNRISE` | **`Mon..Fri`** | `0` | `0`           | `0`/**`true`**  | `0`   |
| Weekends only at sunset           | `SUNSET`  | **`Sat\|Sun`** | `0` | `0`           | `0`/**`true`**  | `0`   |
| Sunset on the 1st of every month  | `SUNSET`  | `0`        | **`1`** | `0`           | `0`/**`true`**  | `0`   |
| Sunrise on the 15th of Jun/Jul/Aug| `SUNRISE` | `0`        | **`15`** | **`Jun\|Jul\|Aug`** | `0`/**`true`** | `0` |
| Sunrise once, tomorrow, then stop | `SUNRISE` | `0`        | `0`   | `0`             | `0`/**`false`** | `0`   |
| Sunset every Mon in 2026 only     | `SUNSET`  | **`Mon`**  | `0`   | `0`             | **`2026`**/`false` | `0` |

> **The daily gotcha:** a solar (or DATE) schedule only recurs when
> `repeat_every_year = true`. Leaving it `false` — with *any* `repeat_days` value,
> including `EVERYDAY` — produces a one-shot that fires a single time. See
> footgun 4 in §5.

### 4.4 `ESP_SCHEDULE_TYPE_RELATIVE`

`relative_seconds` from the base time (`now`, or `validity.start_time` if that is
in the future). Fires exactly **once**. Not recomputed after firing.

### 4.5 Validity window (`esp_schedule_validity_t`) — applies to all types

| Situation                                   | Behavior                                                        | Unit test |
|---------------------------------------------|----------------------------------------------------------------|-----------|
| `start_time` in the future                  | Search origin jumps to `start_time`; result is `>= start_time`. | `validity respected` |
| Next occurrence would be after `end_time`   | Returns no match → schedule expires.                           | `sequence validity cutoff` |
| Callback dispatch delayed past `end_time`   | Trigger is suppressed at fire time (re-check in the timer callback). | N/A (runtime timer path, not unit-testable without a live timer) |
| `start_time == 0` / `end_time == 0` (or NULL) | That bound is disabled (open-ended).                        | `validity respected` |

---

## 5. The short semantic rule

> **Every zero date field means "any". Within a single day, day-of-week and
> day-of-month are OR'd; day, month, and year are AND'd across categories.
> `DATE` and solar triggers both honor day-of-week and day-of-month;
> `DAYS_OF_WEEK` uses only day-of-week. An all-wildcard trigger fires exactly
> once (the one-shot guard); a
> partial pattern repeats until its narrowest bound is exhausted — a specific
> `year` expires it after that year, a `months` mask expires it after the last
> masked month of the current cycle, and `repeat_every_year` never expires.
> Solar triggers follow these exact rules, with the sunrise/sunset instant (± an
> offset) taking the place of the time of day.**

### Quick footguns

1. `day.repeat_days` and `date.day` are combined with **OR**, not AND — there is
   still no way to express "Monday **and** the 15th" (intersection). A corollary:
   `ESP_SCHEDULE_DAY_EVERYDAY` OR'd with any `date.day` matches **every day**
   (EVERYDAY already covers all 7 weekdays), so the day-of-month is silently
   subsumed. For "the 15th only", set `day.repeat_days = 0`. Covered by
   `date everyday subsumes dom`.
2. `day = 0` with a `months` mask means *every day of those months*, not "the
   month(s) only".
3. `repeat_every_year` flips an otherwise one-shot `day`-only DATE (or solar)
   into a forever-repeating schedule.
4. For DATE/solar the one-shot rule **ignores the day-of-week mask**, so
   `repeat_days = 0` and `ESP_SCHEDULE_DAY_EVERYDAY` behave identically — both
   fire once unless `repeat_every_year = true`. To get a *daily* DATE/solar
   schedule, set `repeat_every_year = true`. (This is the opposite of
   `TYPE_DAYS_OF_WEEK`, where `EVERYDAY` repeats and `0` = `DAY_ONCE`.)
