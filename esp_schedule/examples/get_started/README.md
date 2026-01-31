# ESP Schedule Example

## Overview

This example demonstrates how to use the ESP Schedule component to create different types of schedules for ESP32 applications. The example shows four main schedule types:

1. **Days of Week Schedule** - Triggers on specific days of the week at specified times
2. **Date Schedule** - Triggers on specific dates (day and month combinations)
3. **Relative Schedule** - Triggers after a specified number of seconds from creation
4. **Solar Schedule** - Triggers at sunrise or sunset with optional offset

## Schedule Types Demonstrated

### 1. Days of Week Schedule (`ESP_SCHEDULE_TYPE_DAYS_OF_WEEK`)
- Triggers every Monday, Wednesday, and Friday at 14:30 (2:30 PM)
- Useful for recurring weekly events like meetings, reminders, or automated tasks

### 2. Date Schedule (`ESP_SCHEDULE_TYPE_DATE`)
- Triggers every month on the 15th at 09:00 (9:00 AM)
- Useful for monthly recurring events like bill payments, reports, or maintenance tasks
- Can be configured to repeat every year for annual events

### 3. Relative Schedule (`ESP_SCHEDULE_TYPE_RELATIVE`)
- Triggers 60 seconds after creation (one minute timer)
- Useful for one-time delayed actions like turning off a device after a timeout
- Has a validity period (2 minutes in this example)

### 4. Solar Schedule (`ESP_SCHEDULE_TYPE_SUNRISE` / `ESP_SCHEDULE_TYPE_SUNSET`)
- **Sunrise Schedule**: Triggers exactly at sunrise for a specific location (weekdays only)
- **Sunset Schedule**: Triggers 30 minutes before sunset for a specific location (weekdays only)
- Uses latitude/longitude coordinates to calculate solar times with day-of-week filtering
- Perfect for automatic lighting control, irrigation systems, or other location-based automation
- Supports both day-of-week patterns and specific date patterns for solar events
- Requires `CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT=y` to be enabled

### 5. CRON Expression Schedule (`ESP_SCHEDULE_ENABLE_CRON_EXPR`)
- Enable `CONFIG_ESP_SCHEDULE_ENABLE_CRON_EXPR=y`
- Supports directly inputting a cron expression string
- Cron fields order: `minute hour day-of-month month day-of-week` (5-field format)
- Validate your expression with an online checker or unit test to avoid malformed schedules
- Good for recurring jobs such as daily/weekly/monthly reports, maintenance, or notifications

## Features Demonstrated

- **Schedule Creation**: How to create schedules with different trigger types
- **Schedule Persistence**: Schedules are stored in NVS and persist across reboots
- **Network Provisioning**: Automatic WiFi provisioning via BLE or SoftAP for network connectivity with QR code display
- **Time Synchronization**: Robust SNTP time sync with threshold validation for accurate real-world scheduling
- **Callback Functions**: Both trigger callbacks (when schedule fires) and timestamp callbacks (when next trigger time updates)
- **Schedule Management**: Enable, disable, edit, and delete schedules
- **Schedule Recovery**: Automatically recovers and re-enables schedules after reboot

## How to Use Example

### Hardware Required

* An ESP development board (ESP32, ESP32-S2, ESP32-S3, etc.)
* USB cable for power supply and programming
* WiFi network access for time synchronization and network provisioning

### Supported Targets

This example requires Wi-Fi connectivity for network provisioning and time synchronization.

**Supported Targets:** ESP32, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-S2, ESP32-S3

### Network Provisioning

The example uses configurable network provisioning to connect to WiFi:

1. **Flash and run** the example on your ESP device
2. **A QR code will be displayed** in the serial output with provisioning data
3. **Use a BLE/SoftAP provisioning app** (like "ESP SoftAP Prov" on Android/iOS) to scan the QR code (or manually enter the QR code details)
4. **The device will automatically** connect to WiFi and synchronize time via NTP
5. **Schedules will then** operate with accurate real-world timing

#### Configuration

- `CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SCHEME` - provisioning transport type
- `CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION` - provisioning security version:
   - **Version 0**: plaintext communication
   - **Version 1**: Proof of Possession (PoP) used, "12345678"
   - **Version 2**: Username: "wifiprov", Password: "abcd1234"

### Project Configuration

To select the provisioning scheme:

1. Run `idf.py menuconfig` (or `idf.py set-target <chip> && idf.py menuconfig`)
2. Navigate to `Example Configuration` → `ESP Schedule Example Configuration`
3. Select your preferred `Network Provisioning Scheme`:
   - `BLE Provisioning` (default) - Uses Bluetooth LE
   - `SoftAP Provisioning` - Uses WiFi Access Point
4. Save and exit

### Build and Flash

1. Run `idf.py set-target <chip_name>` to set the target chip (e.g., `esp32`, `esp32s3`)
2. Run `idf.py build` to build the project
3. Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

**Note**: The example includes network provisioning (BLE or SoftAP) and SNTP time synchronization which requires network connectivity. The example will automatically provision and connect to WiFi, then synchronize time from NTP servers. Ensure your device can connect to WiFi networks for proper operation.

**Security Note**: The example uses a fixed Proof of Possession (PoP) value of "12345678" for secure provisioning. In production applications, use a unique, randomly generated PoP for enhanced security.

**QR Code Note**: For BLE provisioning, a QR code is displayed in the serial output that can be scanned with the ESP RainMaker app. If the QR code is not visible, copy the provided URL and paste it in a browser to view the QR code.

### Expected Output

The example will show logs like:

```text
ESP Schedule example started. Schedules will trigger based on their configurations.
I (Current time: Fri Oct 24 14:30:00 2025
)
I (Days of week schedule triggered! Data: Monday/Wednesday/Friday schedule
)
```

The schedules will trigger at their configured times:
- **Days of week schedule**: Every Monday, Wednesday, Friday at 14:30
- **Date schedule**: Every 15th of the month at 09:00
- **Relative schedule**: 60 seconds after the program starts
- **Solar schedules**: At sunrise (exactly) and sunset (30 minutes early) for San Francisco, CA (weekdays only)

## Code Structure

### Main Components

- **`esp_schedule_example_main.c`**: Main application file demonstrating schedule creation and usage
- **Callback Functions**: Separate callbacks for each schedule type showing how to handle triggers
- **Schedule Configuration**: Examples of how to configure different schedule types

### Key Functions

- `create_example_schedules()`: Creates the three example schedules
- `edit_example_schedules()`: Demonstrates how to edit existing schedules
- `days_of_week_callback()`, `date_callback()`, `relative_callback()`: Handle schedule triggers
- `timestamp_callback()`: Handles schedule timestamp updates

## Customization

### Changing Schedule Times

To modify the schedule times, edit the `esp_schedule_config_t` structures in `create_example_schedules()`:

```c
// Days of week schedule - change time to 10:00 AM
.trigger.hours = 10,
.trigger.minutes = 0,

// Date schedule - change to 20th of every month at 3:00 PM
.trigger.date.day = 20,
.trigger.hours = 15,
.trigger.minutes = 0,

// Relative schedule - change to 30 seconds from now
.trigger.relative_seconds = 30,

// Solar schedule - change location to New York, adjust timing, and change days
.trigger.day.repeat_days = ESP_SCHEDULE_DAY_SATURDAY | ESP_SCHEDULE_DAY_SUNDAY,  // Weekends only
.trigger.solar.latitude = 40.7128,    // New York latitude
.trigger.solar.longitude = -74.0060,  // New York longitude
.trigger.solar.offset_minutes = 15,   // 15 minutes after sunrise
```

### Adding More Schedule Types

The example demonstrates all the main schedule types including sunrise/sunset schedules. You can also use:

- **Sunrise/Sunset Schedules** (already included in the example - see solar_callback):
  ```c
  .trigger.type = ESP_SCHEDULE_TYPE_SUNRISE,
  .trigger.solar.latitude = 37.7749,    // San Francisco latitude
  .trigger.solar.longitude = -122.4194, // San Francisco longitude
  .trigger.solar.offset_minutes = 30,   // 30 minutes after sunrise
  ```

### Schedule Persistence

Schedules are automatically saved to NVS (Non-Volatile Storage) and will persist across:
- Device reboots
- Power cycles
- Firmware updates

### Schedule Management

The example shows how to:
- Create new schedules
- Enable/disable schedules
- Edit existing schedules
- Handle schedule recovery after reboot

## Troubleshooting

### Schedule Not Triggering

1. **Check system time**: Ensure the ESP32 system time is set correctly
2. **Verify schedule configuration**: Double-check hours/minutes and trigger conditions
3. **Check logs**: Look for schedule creation and trigger messages in the serial output
4. **Solar schedules**: Ensure `CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT=y` is enabled and location coordinates are accurate
5. **Daylight saving time**: Solar schedules automatically adjust for DST changes
6. **CRON expression schedules**: Ensure `CONFIG_ESP_SCHEDULE_ENABLE_CRON_EXPR=y` is enabled, and validate your cron string (e.g., with an online checker) to avoid malformed expressions.

### NVS Issues

If you encounter NVS-related errors:
1. The example includes automatic NVS initialization and recovery
2. Check if the NVS partition has enough space
3. Consider erasing NVS if corruption occurs: `idf.py erase-flash`

### Memory Issues

If you run out of memory when creating many schedules:
1. Each schedule consumes some memory
2. Consider the validity periods to automatically clean up old schedules
3. Monitor heap usage with `ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size())`

## Further Reading

- [ESP Schedule API Reference](../../include/esp_schedule.h)
- [ESP Schedule README](../../README.md)
