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

**Each trigger type reads only the fields it owns.** Setting a field the type does not read is a configuration error, not an ignored value: `esp_schedule_create()` returns `NULL` and `esp_schedule_edit()` returns `ESP_ERR_INVALID_ARG`, with the offending field logged. See §4.7 of [`docs/trigger_rules.md`](docs/trigger_rules.md) for the full rule list.

| Type | Time of day | Day selection |
|------|-------------|---------------|
| `DAYS_OF_WEEK` | `hours`:`minutes` | `day.repeat_days` only |
| `DATE` | `hours`:`minutes` | `date.*` only — no weekday mask |
| `SUNRISE` / `SUNSET` | sunrise/sunset + `solar.offset_minutes` | `day.repeat_days` **or** `date.*`, never both |
| `RELATIVE` | `relative_seconds` | — |

### Day-of-week triggers (ESP_SCHEDULE_TYPE_DAYS_OF_WEEK)

```c
// 05:00 every Monday and Wednesday
esp_schedule_trigger_t t0 = {
    .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    .hours = 5, .minutes = 0,
};
t0.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY;

// 05:00 daily
esp_schedule_trigger_t t1 = {
    .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    .hours = 5, .minutes = 0,
};
t1.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;

// (one-shot) 05:00 at the next occurrence of that time, then done
esp_schedule_trigger_t t2 = {
    .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    .hours = 5, .minutes = 0,
};
t2.day.repeat_days = ESP_SCHEDULE_DAY_ONCE;
```

### Date-based triggers (ESP_SCHEDULE_TYPE_DATE)

Date-based triggers express calendar patterns using:

- **Time of day**: `hours` and `minutes` (24-hour format)
- **Day-of-month**: `date.day` (1–31)
- **Months-of-year mask**: `date.repeat_months` using `esp_schedule_months_t`
- **Specific year**: `date.year` (4-digit), **or** `date.repeat_every_year` — not both

`date.repeat_months` is what makes a date trigger recur — **without a months mask the trigger fires exactly once**, whatever `date.year` says. With a mask, `date.year` / `date.repeat_every_year` decide how far it recurs:

| `date.year` | `date.repeat_every_year` | Recurs until |
|-------------|--------------------------|--------------|
| `0` | `true` | never ends |
| `N` | `false` | end of year `N` |
| `0` | `false` | end of the **current** year — re-resolved on every arm, so a reboot in the next calendar year starts it again for that year |

```c
// 19:30 on the 20th of every month, forever
esp_schedule_trigger_t t3 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 19, .minutes = 30,
};
t3.date.day = 20;                                  // 20th
t3.date.repeat_months = ESP_SCHEDULE_MONTH_ALL;    // every month
t3.date.repeat_every_year = true;                  // <-- recurs over those months

// 14:03 on the 14th of Jan..Apr, every year
esp_schedule_trigger_t t4 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 14, .minutes = 3,
};
t4.date.day = 14;
t4.date.repeat_months = ESP_SCHEDULE_MONTH_JANUARY | ESP_SCHEDULE_MONTH_FEBRUARY |
                        ESP_SCHEDULE_MONTH_MARCH | ESP_SCHEDULE_MONTH_APRIL;
t4.date.repeat_every_year = true;

// 07:00 on the 15th of Jun and Jul 2026, then the schedule expires
esp_schedule_trigger_t t5 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 7, .minutes = 0,
};
t5.date.day = 15;
t5.date.repeat_months = ESP_SCHEDULE_MONTH_JUNE | ESP_SCHEDULE_MONTH_JULY;
t5.date.year = 2026;              // bounds it to 2026 (do not also set repeat_every_year)

// (one-shot) 00:00 on the next 9 August, in 2035
esp_schedule_trigger_t t6 = {
    .type = ESP_SCHEDULE_TYPE_DATE,
    .hours = 0, .minutes = 0,
};
t6.date.day = 9;
t6.date.repeat_months = ESP_SCHEDULE_MONTH_AUGUST;
t6.date.year = 2035;
```

Rejected on purpose: a weekday mask on a `DATE` trigger, `date.repeat_months` without `date.day`, `date.year` together with `date.repeat_every_year`, and `date.repeat_every_year` without a months mask.

> Tip: Prefer using the `esp_schedule_days_t` and `esp_schedule_months_t` constants instead of raw bit values. This keeps code readable and portable.

### Solar triggers

Solar triggers (`ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`) reuse the day selection above — either a weekday mask **or** a date pattern, never both — and replace the time of day with the computed sunrise/sunset instant:

- Provide `solar.latitude`, `solar.longitude`, and an optional `solar.offset_minutes` to shift from the exact sunrise/sunset time.

```c
// Sunset every single day, at the exact sunset time.
esp_schedule_trigger_t s0 = { .type = ESP_SCHEDULE_TYPE_SUNSET };
s0.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY;  // <-- the weekday arm repeats forever
s0.solar.latitude = 37.7749;
s0.solar.longitude = -122.4194;
s0.solar.offset_minutes = 0;      // exactly at sunset

// Sunrise every Monday and Wednesday, 15 minutes before the event
esp_schedule_trigger_t s1 = { .type = ESP_SCHEDULE_TYPE_SUNRISE };
s1.day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY;
s1.solar.latitude = 37.7749;
s1.solar.longitude = -122.4194;
s1.solar.offset_minutes = -15;

// Sunset on the 15th of Feb and Nov, every year, 10 minutes after the event
esp_schedule_trigger_t s2 = { .type = ESP_SCHEDULE_TYPE_SUNSET };
s2.date.day = 15;          // date arm, so no day.repeat_days
s2.date.repeat_months = ESP_SCHEDULE_MONTH_FEBRUARY | ESP_SCHEDULE_MONTH_NOVEMBER;
s2.date.repeat_every_year = true;
s2.solar.latitude = 52.5200;
s2.solar.longitude = 13.4050;
s2.solar.offset_minutes = 10;

// (one-shot) the next sunrise, then done
esp_schedule_trigger_t s3 = { .type = ESP_SCHEDULE_TYPE_SUNRISE };
s3.solar.latitude = 37.7749;
s3.solar.longitude = -122.4194;
```

The day is selected first, then the actual sunrise/sunset instant for that day is computed for your location. Days with no solar event (polar night/day) are skipped.

## Timer Task Stack Requirement

> **Set `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH` to at least 3072.** The 2048-byte default is not enough and overflows the timer daemon task on some IDF versions.

With the default ESP-IDF port, a schedule is driven by a FreeRTOS software timer, so its callback runs on the shared timer daemon task (`Tmr Svc`). A repeating schedule re-arms itself from inside that callback, and the re-arm computes the next occurrence through the date engine and formats it for logging (`localtime_r`, `strftime`) — all on the daemon's stack.

Logging costs one more frame than it looks: `esp_schedule_log()` formats into a 160-byte stack buffer before handing the finished string to `port->log`, and the default port's `ESP_LOGx` then formats a second time. Both frames land on the daemon stack alongside the work above. Lowering the log ceiling compiles those calls out and reclaims the stack with them: either globally via `CONFIG_LOG_MAXIMUM_LEVEL`, or for this component alone by enabling `CONFIG_ESP_SCHEDULE_LOG_LEVEL_OVERRIDE` and setting `CONFIG_ESP_SCHEDULE_LOG_LEVEL`.

An overflow here is not local to your schedule: it takes down the one task that services **every** software timer in the application. It surfaces as

```
***ERROR*** A stack overflow in task Tmr Svc has been detected.
```

Add to `sdkconfig.defaults`:

```
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3072
```

If you supply a [custom port](#custom-ports) that dispatches callbacks on its own task instead, size that task's stack for the same work.

## Port Layer

This component reaches the outside world only through the function pointers declared in [`include/esp_schedule.h`](include/esp_schedule.h):

| Group | Type | Required | Provides |
| --- | --- | --- | --- |
| `timer` | `esp_schedule_timer_ops_t` | yes | one-shot relative timers |
| `time_sync` | `esp_schedule_time_sync_ops_t` | `get_time` only | absolute wall-clock time, optional time sync |
| `mem` | `esp_schedule_mem_ops_t` | yes | `malloc` / `calloc` / `free` |
| `nvs` | `esp_schedule_nvs_ops_t` | no — all-NULL disables persistence | key/value storage |
| `log` | `esp_schedule_log_fn_t` | no — NULL discards output | one pre-formatted line at a time |

### As an ESP-IDF component

`esp_schedule_init()` installs the ESP-IDF implementations, so nothing extra is needed:

- Timers: `port/esp/timer.c` (FreeRTOS software timers)
- Time: `port/esp/time.c` (`time()` and SNTP)
- Memory: `port/esp/mem.c` (libc allocator)
- Storage: `port/esp/nvs.c` (`nvs_flash`)
- Logging: `port/esp/log.c` (`esp_log`)

### Mixing the defaults with your own

The default tables are public, in [`include/esp_schedule_esp_port.h`](include/esp_schedule_esp_port.h), so a port need not be written from scratch. Keep the groups you want and replace the rest:

```c
#include "esp_schedule_esp_port.h"

esp_schedule_port_config_t port = {
    .timer = esp_schedule_esp_timer_ops,
    .nvs   = esp_schedule_esp_nvs_ops,
    .time_sync  = esp_schedule_esp_time_sync_ops,
    .mem   = esp_schedule_esp_mem_ops,
    .log   = esp_schedule_esp_log,
};

port.time_sync.timesync_init = NULL;   /* keep time(), do not start SNTP */
port.mem = my_pool_ops;           /* or swap a whole group out */

uint8_t count = 0;
esp_schedule_handle_t *restored = esp_schedule_init_with_config(&port, true, NULL, &count);
```

Individual members can be overridden like this, but only where the table says the member is optional — `time_sync.get_time` and every member of `timer` and `mem` are required, and `esp_schedule_init_with_config()` rejects a table missing one with `ESP_ERR_INVALID_ARG`.

You pay for what you name: referencing a table links its implementation and that implementation's dependency. `test_app/main/test_app_main.c` uses exactly this pattern.

> **SNTP:** nulling `timesync_init` stops SNTP being *started*, but naming `esp_schedule_esp_time_sync_ops` still links `port/esp/time.c` and with it `esp_sntp`. To remove the dependency rather than just the call, set `CONFIG_ESP_SCHEDULE_ENABLE_SNTP=n` — the default table then carries a NULL `timesync_init` on its own and `esp_sntp` is not referenced at all. Use that if the clock comes from an RTC or is set by the application.

### Custom ports

Fill in an `esp_schedule_port_config_t` and call `esp_schedule_init_with_config()` instead of `esp_schedule_init()`. No custom `CMakeLists.txt` is involved:

```c
static const esp_schedule_port_config_t port = {
    .timer = { .start = my_timer_start, .stop = my_timer_stop, .cancel = my_timer_cancel },
    .time_sync  = { .get_time = my_get_time },
    .mem   = { .malloc = my_malloc, .calloc = my_calloc, .free = my_free },
    .log   = my_log,   /* optional */
    /* .nvs left zeroed: schedules live only for this run */
};

uint8_t count = 0;
esp_schedule_handle_t *restored = esp_schedule_init_with_config(&port, false, NULL, &count);
/* Allocated with port.mem.malloc, so release it with port.mem.free - not libc
 * free(), unless your port happens to wrap the libc allocator. */
my_free(restored);
```

The port is validated once at install time and copied by value, so it may be built on the stack. `esp_schedule_init()` is the only referrer of the ESP-IDF implementations, so an application that calls only `esp_schedule_init_with_config()` never links them — no FreeRTOS timer, `nvs_flash`, SNTP or `esp_log` dependency is pulled in on this component's behalf. Set `CONFIG_ESP_SCHEDULE_DISABLE_DEFAULT_PORT=y` to stop compiling them altogether.
