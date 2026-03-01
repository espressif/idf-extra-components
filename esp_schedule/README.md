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

## Triggers as a list

Schedules can now contain multiple triggers. Instead of a single `trigger`, use `triggers` with a list and count. Each entry is an `esp_schedule_trigger_t` and the scheduler will trigger on the union of all entries.

```
#include "esp_schedule.h"

static void my_trigger_cb(esp_schedule_handle_t handle, void *priv_data) {
    // Handle trigger
}

void app_create_multi_trigger_schedule(void) {
    static esp_schedule_trigger_t trigger_list[3];

    // 1) Every Monday and Thursday at 13:30
    trigger_list[0].type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    trigger_list[0].hours = 13;
    trigger_list[0].minutes = 30;
    trigger_list[0].day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_THURSDAY;

    // 2) Date-based: 19:30 on the 20th day of any month
    trigger_list[1].type = ESP_SCHEDULE_TYPE_DATE;
    trigger_list[1].hours = 19;
    trigger_list[1].minutes = 30;
    trigger_list[1].date.day = 20;                    // 20th of the month
    trigger_list[1].date.repeat_months = 0;           // any month
    trigger_list[1].date.year = 0;                    // any year
    trigger_list[1].date.repeat_every_year = true;    // keep repeating

    // 3) Sunrise with 15 minutes offset before, only on weekends
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    trigger_list[2].type = ESP_SCHEDULE_TYPE_SUNRISE;
    trigger_list[2].hours = 0;    // ignored by solar types
    trigger_list[2].minutes = 0;  // ignored by solar types
    trigger_list[2].day.repeat_days = ESP_SCHEDULE_DAY_SATURDAY | ESP_SCHEDULE_DAY_SUNDAY;
    trigger_list[2].date.day = 0;                 // use day-of-week pattern
    trigger_list[2].date.repeat_months = 0;       // any month
    trigger_list[2].date.year = 0;                // any year
    trigger_list[2].date.repeat_every_year = true;
    trigger_list[2].solar.latitude = 37.7749;     // San Francisco
    trigger_list[2].solar.longitude = -122.4194;
    trigger_list[2].solar.offset_minutes = -15;   // 15 minutes before sunrise
#endif

    esp_schedule_config_t cfg = { 0 };
    strncpy(cfg.name, "multi", sizeof(cfg.name) - 1);
    cfg.triggers.list = trigger_list;
    cfg.triggers.count = sizeof(trigger_list) / sizeof(trigger_list[0]);
    cfg.trigger_cb = my_trigger_cb;

    esp_schedule_handle_t h = esp_schedule_create(&cfg);
    (void)h;
}
```

## Date-based triggers (ESP_SCHEDULE_TYPE_DATE)

Date-based triggers provide a flexible way to express calendar patterns using a combination of:

- **Time of day**: `hours` and `minutes` (24-hour format)
- **Day-of-week mask**: `day.repeat_days` using `esp_schedule_days_t`
- **Day-of-month**: `date.day` (1–31)
- **Months-of-year mask**: `date.repeat_months` using `esp_schedule_months_t`
- **Specific year**: `date.year` (4-digit), with `date.repeat_every_year`

Rules:
- **At least one** of day-of-week (`day.repeat_days`) or day-of-month (`date.day`) should be set.
- Day-of-week and day-of-month are combined using **OR**: a date matches if it is on one of the specified weekdays OR it is the specified day of the month.
- Months and year act as additional filters. If a months mask is set, the date must be within those months. If a year is set (non-zero), only that specific year will match. Use `repeat_every_year = true` when you want the pattern to continue in subsequent years.

Examples (equivalent to common JSON patterns):

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

## Solar triggers with the same flexibility

Solar triggers (`ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`) support the same filtering patterns as date-based triggers:

- Use `day.repeat_days` for day-of-week filters.
- Use `date.day`, `date.repeat_months`, and `date.year` for specific date/month/year patterns.
- Provide `solar.latitude`, `solar.longitude`, and an optional `solar.offset_minutes` to shift from the exact sunrise/sunset time.

Examples:

```c
// Sunrise every Monday, Wednesday 15 minutes before event
esp_schedule_trigger_t s1 = { .type = ESP_SCHEDULE_TYPE_SUNRISE };
s1.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY;
s1.date.day = 0; // using day-of-week pattern
s1.date.repeat_months = 0; // any month
s1.date.year = 0;
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

## Glue Layers

This component makes use of the following glue abstraction layers under `glue`:
- `glue_log.h`: Logging
- `glue_mem.h`: Memory allocation
- `glue_nvs.h`: Non-Volatile Storage
- `glue_time.h`: Time provider and synchronization
- `glue_timer.h`: Timer implementation

### As an ESP-IDF component

When using this component normally, the default glue implementations are used:
- Logging: `glue/esp/glue_log_impl.h` 
- Memory allocation: `glue/esp/glue_mem_impl.h`
- Non-Volatile Storage: `glue/esp/nvs.c`
- Time provider and synchronization: `glue/esp/time.c`
- Timer implementation: `glue/esp/timer.c`

### Custom glue implementations

If the underlying implementations are required to be changed, then you would need to implement a custom `CMakeLists.txt` for this component:
1. Common non-glue sources and include directories can be included using `esp_schedule_variables.cmake`.
2. Append your glue sources and include directories to the variables provided.
3. Use the variables to build your target library (e.g., passing them to `idf_component_register`).
4. (Optional) If you wish to **enable NVS**, and the configuration system used is *not compatible with Kconfig files*:
    - You must specify `#define CONFIG_ESP_SCHEDULE_ENABLE_NVS 1` **very early at the top-level include file**.
    -  Not doing so will *disable NVS by default*.

The [default implementation](#as-an-esp-idf-component) does this in this component's `CMakeLists.txt` with the default glue implementations.