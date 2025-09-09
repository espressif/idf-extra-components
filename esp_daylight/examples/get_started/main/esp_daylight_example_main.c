/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "esp_log.h"
#include "esp_daylight.h"

static const char *TAG = "esp_daylight_example";

/* Example locations around the world */
static const esp_daylight_location_t example_locations[] = {
    {40.7128, -74.0060, "New York, USA"},
    {51.5074, -0.1278, "London, UK"},
    {18.5204, 73.8567, "Pune, India"},
    {31.2304, 121.4737, "Shanghai, China"},
    {-33.8688, 151.2093, "Sydney, Australia"},
    {55.7558, 37.6173, "Moscow, Russia"},
    {35.6762, 139.6503, "Tokyo, Japan"},
    {-22.9068, -43.1729, "Rio de Janeiro, Brazil"}
};

static const size_t num_locations = sizeof(example_locations) / sizeof(example_locations[0]);

/* Helper function to format time as string */
static void format_time_string(time_t timestamp, char *buffer, size_t buffer_size)
{
    struct tm *time_info = gmtime(&timestamp);
    strftime(buffer, buffer_size, "%H:%M:%S UTC", time_info);
}

/* Helper function to calculate and display daylight duration */
static void display_daylight_info(const esp_daylight_location_t *location,
                                  int year, int month, int day)
{
    time_t sunrise_utc, sunset_utc;
    char sunrise_str[32], sunset_str[32];

    bool result = esp_daylight_calc_sunrise_sunset_location(
                      year, month, day, location, &sunrise_utc, &sunset_utc
                  );

    if (result) {
        format_time_string(sunrise_utc, sunrise_str, sizeof(sunrise_str));
        format_time_string(sunset_utc, sunset_str, sizeof(sunset_str));

        /* Calculate daylight duration */
        int daylight_seconds = (int)(sunset_utc - sunrise_utc);

        /* Handle day boundary crossing (sunset next day) */
        if (daylight_seconds < 0) {
            daylight_seconds += 24 * 60 * 60; /* Add 24 hours */
        }

        int daylight_minutes = daylight_seconds / 60;
        int hours = daylight_minutes / 60;
        int minutes = daylight_minutes % 60;

        ESP_LOGI(TAG, "%-20s: Sunrise %s, Sunset %s (Daylight: %02d:%02d)",
                 location->name, sunrise_str, sunset_str, hours, minutes);
    } else {
        ESP_LOGI(TAG, "%-20s: No sunrise/sunset (polar day/night)", location->name);
    }
}

/* Demonstrate basic sunrise/sunset calculation */
static void example_basic_calculation(void)
{
    ESP_LOGI(TAG, "=== Basic Sunrise/Sunset Calculation ===");

    /* Calculate for today's date (example: August 29, 2025) */
    int year = 2025, month = 8, day = 29;

    ESP_LOGI(TAG, "Calculating sunrise/sunset for %04d-%02d-%02d:", year, month, day);
    ESP_LOGI(TAG, "");

    for (size_t i = 0; i < num_locations; i++) {
        display_daylight_info(&example_locations[i], year, month, day);
    }
    ESP_LOGI(TAG, "");
}

/* Demonstrate seasonal variations */
static void example_seasonal_variations(void)
{
    ESP_LOGI(TAG, "=== Seasonal Variations Example ===");

    /* Use London as example location */
    const esp_daylight_location_t *london = &example_locations[1];

    /* Test different seasons */
    struct {
        int month, day;
        const char *season;
    } seasons[] = {
        {3, 21, "Spring Equinox"},
        {6, 21, "Summer Solstice"},
        {9, 23, "Autumn Equinox"},
        {12, 21, "Winter Solstice"}
    };

    ESP_LOGI(TAG, "Seasonal daylight variations in %s (2025):", london->name);
    ESP_LOGI(TAG, "");

    for (size_t i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "%s (%02d-%02d):", seasons[i].season, seasons[i].month, seasons[i].day);
        display_daylight_info(london, 2025, seasons[i].month, seasons[i].day);
        ESP_LOGI(TAG, "");
    }
}

/* Demonstrate time offset functionality */
static void example_time_offsets(void)
{
    ESP_LOGI(TAG, "=== Time Offset Example ===");

    /* Use Pune as example */
    const esp_daylight_location_t *pune = &example_locations[2];
    time_t sunrise_utc, sunset_utc;

    bool result = esp_daylight_calc_sunrise_sunset_location(
                      2025, 8, 29, pune, &sunrise_utc, &sunset_utc
                  );

    if (result) {
        char time_str[32];

        ESP_LOGI(TAG, "Original times for %s:", pune->name);
        format_time_string(sunrise_utc, time_str, sizeof(time_str));
        ESP_LOGI(TAG, "  Sunrise: %s", time_str);
        format_time_string(sunset_utc, time_str, sizeof(time_str));
        ESP_LOGI(TAG, "  Sunset:  %s", time_str);
        ESP_LOGI(TAG, "");

        /* Apply various offsets */
        struct {
            int offset_minutes;
            const char *description;
        } offsets[] = {
            {-30, "30 minutes before sunset (lights on)"},
            {30, "30 minutes after sunrise (morning routine)"},
            {-60, "1 hour before sunset (dinner prep)"},
            {15, "15 minutes after sunrise (wake up)"}
        };

        ESP_LOGI(TAG, "Time offset examples:");
        for (size_t i = 0; i < 4; i++) {
            time_t base_time = (i % 2 == 0) ? sunset_utc : sunrise_utc;
            time_t offset_time = esp_daylight_apply_offset(base_time, offsets[i].offset_minutes);

            format_time_string(offset_time, time_str, sizeof(time_str));
            ESP_LOGI(TAG, "  %s: %s", offsets[i].description, time_str);
        }
        ESP_LOGI(TAG, "");
    }
}

/* Demonstrate polar region handling */
static void example_polar_regions(void)
{
    ESP_LOGI(TAG, "=== Polar Region Example ===");

    /* Test Arctic locations */
    esp_daylight_location_t arctic_locations[] = {
        {71.0, 8.0, "Svalbard, Norway"},
        {80.0, 0.0, "High Arctic"},
        {-77.8, 166.7, "McMurdo, Antarctica"}
    };

    /* Test summer and winter conditions */
    struct {
        int month, day;
        const char *season;
    } polar_seasons[] = {
        {6, 21, "Summer (Midnight Sun)"},
        {12, 21, "Winter (Polar Night)"}
    };

    for (size_t s = 0; s < 2; s++) {
        ESP_LOGI(TAG, "%s conditions:", polar_seasons[s].season);

        for (size_t i = 0; i < 3; i++) {
            time_t sunrise_utc, sunset_utc;
            bool result = esp_daylight_calc_sunrise_sunset_location(
                              2025, polar_seasons[s].month, polar_seasons[s].day,
                              &arctic_locations[i], &sunrise_utc, &sunset_utc
                          );

            if (result) {
                char sunrise_str[32], sunset_str[32];
                format_time_string(sunrise_utc, sunrise_str, sizeof(sunrise_str));
                format_time_string(sunset_utc, sunset_str, sizeof(sunset_str));
                ESP_LOGI(TAG, "  %-20s: Sunrise %s, Sunset %s",
                         arctic_locations[i].name, sunrise_str, sunset_str);
            } else {
                ESP_LOGI(TAG, "  %-20s: No sunrise/sunset (24h %s)",
                         arctic_locations[i].name,
                         (polar_seasons[s].month == 6) ? "daylight" : "darkness");
            }
        }
        ESP_LOGI(TAG, "");
    }
}

/* Demonstrate practical scheduling use case */
static void example_practical_scheduling(void)
{
    ESP_LOGI(TAG, "=== Practical Scheduling Example ===");

    /* Simulate a smart home lighting system */
    const esp_daylight_location_t *home_location = &example_locations[2]; /* Pune */
    time_t sunrise_utc, sunset_utc;

    bool result = esp_daylight_calc_sunrise_sunset_location(
                      2025, 8, 29, home_location, &sunrise_utc, &sunset_utc
                  );

    if (result) {
        ESP_LOGI(TAG, "Smart Home Lighting Schedule for %s:", home_location->name);
        ESP_LOGI(TAG, "");

        /* Define lighting events */
        struct {
            time_t event_time;
            const char *action;
            const char *description;
        } lighting_events[6];

        /* Calculate event times */
        lighting_events[0].event_time = esp_daylight_apply_offset(sunrise_utc, -30);
        lighting_events[0].action = "Turn OFF";
        lighting_events[0].description = "30 min before sunrise";

        lighting_events[1].event_time = sunrise_utc;
        lighting_events[1].action = "Dim to 20%";
        lighting_events[1].description = "At sunrise";

        lighting_events[2].event_time = esp_daylight_apply_offset(sunrise_utc, 60);
        lighting_events[2].action = "Turn OFF";
        lighting_events[2].description = "1 hour after sunrise";

        lighting_events[3].event_time = esp_daylight_apply_offset(sunset_utc, -45);
        lighting_events[3].action = "Turn ON 50%";
        lighting_events[3].description = "45 min before sunset";

        lighting_events[4].event_time = sunset_utc;
        lighting_events[4].action = "Turn ON 80%";
        lighting_events[4].description = "At sunset";

        lighting_events[5].event_time = esp_daylight_apply_offset(sunset_utc, 120);
        lighting_events[5].action = "Turn ON 100%";
        lighting_events[5].description = "2 hours after sunset";

        /* Display schedule */
        for (size_t i = 0; i < 6; i++) {
            char time_str[32];
            format_time_string(lighting_events[i].event_time, time_str, sizeof(time_str));
            ESP_LOGI(TAG, "  %s - %-15s (%s)",
                     time_str, lighting_events[i].action, lighting_events[i].description);
        }
        ESP_LOGI(TAG, "");

        /* Show integration with scheduling system */
        ESP_LOGI(TAG, "Integration with ESP Schedule:");
        ESP_LOGI(TAG, "  esp_schedule_config_t config = {");
        ESP_LOGI(TAG, "      .name = \"smart_lighting\",");
        ESP_LOGI(TAG, "      .trigger.type = ESP_SCHEDULE_TYPE_SUNSET,");
        ESP_LOGI(TAG, "      .trigger.solar.latitude = %.4f,", home_location->latitude);
        ESP_LOGI(TAG, "      .trigger.solar.longitude = %.4f,", home_location->longitude);
        ESP_LOGI(TAG, "      .trigger.solar.offset_minutes = -45,");
        ESP_LOGI(TAG, "      .trigger_cb = lighting_control_callback,");
        ESP_LOGI(TAG, "      .timestamp_cb = schedule_timestamp_callback");
        ESP_LOGI(TAG, "  };");
        ESP_LOGI(TAG, "  esp_schedule_handle_t handle = esp_schedule_create(&config);");
        ESP_LOGI(TAG, "  esp_schedule_enable(handle);");
        ESP_LOGI(TAG, "");
        // Note: The above is a demonstration of how to configure the schedule.
        // Actual integration would require the esp_schedule component and real callback implementations.
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP Daylight Component Example");
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, "");

    /* Run all examples */
    example_basic_calculation();
    example_seasonal_variations();
    example_time_offsets();
    example_polar_regions();
    example_practical_scheduling();

    ESP_LOGI(TAG, "Example completed successfully!");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Next steps:");
    ESP_LOGI(TAG, "- Modify coordinates to match your location");
    ESP_LOGI(TAG, "- Integrate with your scheduling system");
    ESP_LOGI(TAG, "- Add timezone conversion for local time display");
    ESP_LOGI(TAG, "- Implement automated lighting/irrigation control");
}
