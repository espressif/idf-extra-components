# ESP Daylight Example

This example demonstrates how to use the ESP Daylight component to calculate sunrise and sunset times for various locations around the world.

## What This Example Does

The example showcases several key features of the ESP Daylight component:

1. **Basic Calculations**: Calculate sunrise/sunset for multiple cities worldwide
2. **Seasonal Variations**: Show how daylight hours change throughout the year
3. **Time Offsets**: Demonstrate scheduling events relative to sunrise/sunset
4. **Polar Regions**: Handle special cases like midnight sun and polar night
5. **Practical Scheduling**: Real-world smart home lighting automation example

## How to Use

### Build and Flash

Create the example project:

```bash
idf.py create-project-from-example "espressif/esp_daylight:get_started"
cd get_started
idf.py set-target esp32
idf.py build flash monitor
```

Alternatively, if you have the component source locally:

```bash
cd examples/get_started
idf.py set-target esp32
idf.py build flash monitor
```

### Expected Output

The example will display sunrise and sunset times for various locations and demonstrate different use cases:

```
ESP Daylight Component Example
============================

=== Basic Sunrise/Sunset Calculation ===
Calculating sunrise/sunset for 2025-08-29:

New York, USA       : Sunrise 10:12:34 UTC, Sunset 23:45:12 UTC (Daylight: 13:32)
London, UK          : Sunrise 05:23:45 UTC, Sunset 19:34:56 UTC (Daylight: 14:11)
Pune, India         : Sunrise 01:15:23 UTC, Sunset 13:02:45 UTC (Daylight: 11:47)
...
```

## Key Locations Tested

The example includes calculations for these major cities:
- New York, USA (40.7128°N, 74.0060°W)
- London, UK (51.5074°N, 0.1278°W)
- Pune, India (18.5204°N, 73.8567°E)
- Shanghai, China (31.2304°N, 121.4737°E)
- Sydney, Australia (33.8688°S, 151.2093°E)
- Moscow, Russia (55.7558°N, 37.6173°E)
- Tokyo, Japan (35.6762°N, 139.6503°E)
- Rio de Janeiro, Brazil (22.9068°S, 43.1729°W)

## Customization

### Change Location

Modify the coordinates in the code to match your location:

```c
esp_daylight_location_t my_location = {
    .latitude = YOUR_LATITUDE,
    .longitude = YOUR_LONGITUDE,
    .name = "My Location"
};
```

### Change Date

Update the date parameters in the calculation functions:

```c
int year = 2025, month = 8, day = 29;  // Change to your desired date
```

### Add Time Zone Support

The component returns UTC timestamps. To display local time, you can convert using standard C library functions or ESP-IDF timezone support.

## Integration with Scheduling

The example shows how to integrate with scheduling systems:

```c
// Calculate sunset time
time_t sunset_utc;
esp_daylight_calc_sunrise_sunset_utc(2025, 8, 29, lat, lon, NULL, &sunset_utc);

// Schedule event 30 minutes before sunset
time_t light_on_time = esp_daylight_apply_offset(sunset_utc, -30);

// Use with ESP Schedule component (if available)
esp_schedule_config_t config = {
    .trigger.type = ESP_SCHEDULE_TYPE_SUNSET,
    .trigger.solar.latitude = lat,
    .trigger.solar.longitude = lon,
    .trigger.solar.offset_minutes = -30,
    .callback = your_callback_function
};
```

## Troubleshooting

### No Output for Polar Regions

If you see "No sunrise/sunset" messages, this is normal for polar regions during certain times of year (midnight sun in summer, polar night in winter).

### Accuracy

The calculations use NOAA Solar Calculator equations and are typically accurate to within 1-2 minutes for most locations.

## Next Steps

- Modify coordinates for your specific location
- Integrate with your IoT scheduling system
- Add timezone conversion for local time display
- Implement automated device control based on solar events
- Create recurring schedules that automatically adjust throughout the year

