# ESP Scheduling

[![Component Registry](https://components.espressif.com/components/espressif/esp_schedule/badge.svg)](https://components.espressif.com/components/espressif/esp_schedule)

This component is used to implement scheduling for:

- **One-shot events** with a relative time difference (e.g., 30 seconds into the future)
- **Periodic events** based on a certain time[^1] on days of the week (e.g., every Monday or Wednesday)
- **Periodic/one-shot events** on a certain time[^1] based on the date:
  - e.g., *(periodic)* every 23rd of January to April
  - e.g., *(one-shot)* 9th of August, 2026
- **Periodic events** at an offset from sunrise/sunset

[^1]: By default, the time is w.r.t. UTC. If the timezone has been set, then the time is w.r.t. the specified timezone.

## Example Usage

See the comprehensive example in [`examples/get_started/`](examples/get_started/) for a complete demonstration of all ESP Schedule features, including:

- **Days of Week Scheduling** - Recurring events on specific weekdays
- **Date-based Scheduling** - Monthly and yearly recurring events
- **Relative Scheduling** - One-time delayed events
- **Solar Scheduling** - Sunrise/sunset based events with location coordinates and day-of-week filtering
- **Schedule Persistence** - NVS storage and recovery
- **Callback Handling** - Trigger and timestamp callbacks
- **Schedule Management** - Create, edit, enable, and disable schedules

The example includes detailed documentation, build instructions, and demonstrates all schedule types with practical use cases.

## Trigger Usage

> **Full specification:** see [`docs/trigger_rules.md`](docs/trigger_rules.md) for the date engine's exact behavior, the day-match / one-shot rules, DST handling, and a comprehensive scenario table for every field combination.

### Date-based triggers (ESP_SCHEDULE_TYPE_DATE)

Date-based triggers provide a flexible way to express calendar patterns using a combination of:

- **Time of day**: `hours` and `minutes` (24-hour format)
- **Day-of-week mask**: `day.repeat_days` using `esp_schedule_days_t`
- **Day-of-month**: `date.day` (1–31)
- **Months-of-year mask**: `date.repeat_months` using `esp_schedule_months_t`
- **Specific year**: `date.year` (4-digit), with `date.repeat_every_year`

Quick examples:

```c
// 05:00 every Monday and Wednesday
esp_schedule_trigger_t t1 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 5, .minutes = 0,
};
t1.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY;
t1.date.day = 0;                  // not using day-of-month
t1.date.repeat_months = 0;        // any month
t1.date.year = 0;                 // any year
t1.date.repeat_every_year = true;

// 19:30 every 20th of the month
esp_schedule_trigger_t t2 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 19, .minutes = 30,
};
t2.day.repeat_days = 0;           // any day-of-week
t2.date.day = 20;                 // 20th
t2.date.repeat_months = 0;        // any month
t2.date.year = 0;                 // any year
t2.date.repeat_every_year = true;

// 14:03 on (Tue..Sat) OR on the 14th, limited to Jan..Apr
esp_schedule_trigger_t t3 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 14, .minutes = 3,
};
t3.day.repeat_days = ESP_SCHEDULE_DAY_TUESDAY | ESP_SCHEDULE_DAY_WEDNESDAY |
                     ESP_SCHEDULE_DAY_THURSDAY | ESP_SCHEDULE_DAY_FRIDAY |
                     ESP_SCHEDULE_DAY_SATURDAY;
t3.date.day = 14;                 // OR 14th of the month
t3.date.repeat_months = ESP_SCHEDULE_MONTH_JANUARY | ESP_SCHEDULE_MONTH_FEBRUARY |
                        ESP_SCHEDULE_MONTH_MARCH | ESP_SCHEDULE_MONTH_APRIL;
t3.date.year = 0;                 // any year
t3.date.repeat_every_year = true;

// (one-shot) 00:00 on 9 August 2035
esp_schedule_trigger_t t4 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 0, .minutes = 0,
};
t4.day.repeat_days = 0;           // not used
t4.date.day = 9;                  // 9th
t4.date.repeat_months = ESP_SCHEDULE_MONTH_AUGUST;
t4.date.year = 2035;              // specific year
t4.date.repeat_every_year = false; // one-shot in that year
```

> Tip: Prefer using the `esp_schedule_days_t` and `esp_schedule_months_t` constants instead of raw bit values. This keeps code readable and portable.

### Solar triggers with the same flexibility

Solar triggers (`ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`) behave **exactly like date-based triggers** — same day-of-week / day-of-month / months / year / `repeat_every_year` rules — except the time of day is replaced by the computed sunrise/sunset instant:

- Provide `solar.latitude`, `solar.longitude`, and an optional `solar.offset_minutes` to shift from the exact sunrise/sunset time.

Quick examples:

```c
// Sunset every single day, at the exact sunset time.
// NOTE: repeat_every_year = true is what makes it recur. With any day pattern
// (repeat_days = 0, EVERYDAY, or a specific mask) but repeat_every_year = false,
// a solar/DATE trigger is a ONE-SHOT and fires only once. repeat_days = 0 and
// ESP_SCHEDULE_DAY_EVERYDAY are equivalent here (both mean "every day").
esp_schedule_trigger_t s0 = { .type = ESP_SCHEDULE_TYPE_SUNSET };
s0.day.repeat_days = 0;           // any day-of-week (== EVERYDAY for date/solar)
s0.date.day = 0;                  // not using day-of-month
s0.date.repeat_months = 0;        // any month
s0.date.year = 0;                 // any year
s0.date.repeat_every_year = true; // <-- required for daily recurrence
s0.solar.latitude = 37.7749;
s0.solar.longitude = -122.4194;
s0.solar.offset_minutes = 0;      // exactly at sunset

// Sunrise every Monday, Wednesday 15 minutes before event
esp_schedule_trigger_t s1 = { .type = ESP_SCHEDULE_TYPE_SUNRISE };
s1.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY;
s1.date.day = 0; // using day-of-week pattern
s1.date.repeat_months = 0; // any month
s1.date.year = 0;                 // any year
s1.date.repeat_every_year = true;
s1.solar.latitude = 37.7749;
s1.solar.longitude = -122.4194;
s1.solar.offset_minutes = -15;

// Sunset on the 15th day of Feb and Nov, every year, 10 minutes after event
esp_schedule_trigger_t s2 = { .type = ESP_SCHEDULE_TYPE_SUNSET };
s2.day.repeat_days = 0;    // not using day-of-week filter
s2.date.day = 15;          // 15th of the month
s2.date.repeat_months = ESP_SCHEDULE_MONTH_FEBRUARY | ESP_SCHEDULE_MONTH_NOVEMBER;
s2.date.year = 0;          // any year
s2.date.repeat_every_year = true;
s2.solar.latitude = 52.5200;
s2.solar.longitude = 13.4050;
s2.solar.offset_minutes = 10;
```

These filters are applied when computing the next valid sunrise/sunset for your location. If both day-of-week and day-of-month are specified, they are combined using **OR**; month and year remain additional filters.
