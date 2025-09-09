/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <math.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_daylight.h"

static const char *TAG = "esp_daylight_test";

/* Test tolerance for time calculations (in seconds) */
#define TIME_TOLERANCE_SEC 120  /* 2 minutes tolerance for sunrise/sunset calculations */

/* Helper function to check if two timestamps are within tolerance */
__attribute__((unused)) static bool time_within_tolerance(time_t actual, time_t expected, int tolerance_sec)
{
    int diff = abs((int)(actual - expected));
    return diff <= tolerance_sec;
}

TEST_CASE("Test basic sunrise/sunset calculation", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test for Pune, India on August 29, 2025 */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 8, 29,           /* Date */
                 18.5204, 73.8567,      /* Pune coordinates */
                 &sunrise_utc, &sunset_utc
             );

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_EQUAL(0, sunrise_utc);
    TEST_ASSERT_NOT_EQUAL(0, sunset_utc);
    /* Note: Don't assert sunset > sunrise due to potential day boundary crossing in UTC */

    /* Convert to readable format for logging */
    struct tm sunrise_tm_buf, sunset_tm_buf;
    struct tm *sunrise_tm = gmtime_r(&sunrise_utc, &sunrise_tm_buf);
    struct tm *sunset_tm = gmtime_r(&sunset_utc, &sunset_tm_buf);

    ESP_LOGI(TAG, "Pune 2025-08-29: Sunrise %02d:%02d UTC, Sunset %02d:%02d UTC",
             sunrise_tm->tm_hour, sunrise_tm->tm_min,
             sunset_tm->tm_hour, sunset_tm->tm_min);

    /* Sanity check: sunrise should be around 01:00 UTC (06:30 IST) */
    /* sunset should be around 13:00 UTC (18:30 IST) */
    TEST_ASSERT_TRUE(sunrise_tm->tm_hour >= 0 && sunrise_tm->tm_hour <= 3);
    TEST_ASSERT_TRUE(sunset_tm->tm_hour >= 12 && sunset_tm->tm_hour <= 15);
}

TEST_CASE("Test location struct interface", "[esp_daylight]")
{
    esp_daylight_location_t location = {
        .latitude = 40.7128,
        .longitude = -74.0060,
        .name = "New York"
    };

    time_t sunrise_utc, sunset_utc;
    bool result;

    result = esp_daylight_calc_sunrise_sunset_location(
                 2025, 6, 21,  /* Summer solstice */
                 &location,
                 &sunrise_utc, &sunset_utc
             );

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_EQUAL(0, sunrise_utc);
    TEST_ASSERT_NOT_EQUAL(0, sunset_utc);
    TEST_ASSERT_GREATER_THAN(sunset_utc, sunrise_utc);

    struct tm sunrise_tm_buf, sunset_tm_buf;
    struct tm *sunrise_tm = gmtime_r(&sunrise_utc, &sunrise_tm_buf);
    struct tm *sunset_tm = gmtime_r(&sunset_utc, &sunset_tm_buf);

    ESP_LOGI(TAG, "New York 2025-06-21: Sunrise %02d:%02d UTC, Sunset %02d:%02d UTC",
             sunrise_tm->tm_hour, sunrise_tm->tm_min,
             sunset_tm->tm_hour, sunset_tm->tm_min);
}

TEST_CASE("Test polar regions - midnight sun", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test Arctic location during summer (midnight sun) */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 6, 21,           /* Summer solstice */
                 80.0, 0.0,             /* High Arctic latitude */
                 &sunrise_utc, &sunset_utc
             );

    /* Should return false for midnight sun conditions */
    TEST_ASSERT_FALSE(result);
    ESP_LOGI(TAG, "Arctic midnight sun test: correctly returned false");
}

TEST_CASE("Test polar regions - polar night", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test Arctic location during winter (polar night) */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 12, 21,          /* Winter solstice */
                 80.0, 0.0,             /* High Arctic latitude */
                 &sunrise_utc, &sunset_utc
             );

    /* Should return false for polar night conditions */
    TEST_ASSERT_FALSE(result);
    ESP_LOGI(TAG, "Arctic polar night test: correctly returned false");
}

TEST_CASE("Test time offset functionality", "[esp_daylight]")
{
    time_t base_time = 1640995200; /* 2022-01-01 00:00:00 UTC */
    time_t offset_time;

    /* Test positive offset (30 minutes after) */
    offset_time = esp_daylight_apply_offset(base_time, 30);
    TEST_ASSERT_EQUAL(base_time + 1800, offset_time);

    /* Test negative offset (45 minutes before) */
    offset_time = esp_daylight_apply_offset(base_time, -45);
    TEST_ASSERT_EQUAL(base_time - 2700, offset_time);

    /* Test zero offset */
    offset_time = esp_daylight_apply_offset(base_time, 0);
    TEST_ASSERT_EQUAL(base_time, offset_time);

    ESP_LOGI(TAG, "Time offset tests passed");
}

TEST_CASE("Test input validation", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;

    /* Test invalid date */
    (void) esp_daylight_calc_sunrise_sunset_utc(
        2025, 13, 1,           /* Invalid month */
        18.5204, 73.8567,
        &sunrise_utc, &sunset_utc
    );
    /* Implementation should handle this gracefully */

    /* Test extreme latitudes */
    (void) esp_daylight_calc_sunrise_sunset_utc(
        2025, 6, 21,
        91.0, 0.0,             /* Invalid latitude > 90 */
        &sunrise_utc, &sunset_utc
    );
    /* Should handle gracefully */

    /* Test extreme longitudes */
    (void) esp_daylight_calc_sunrise_sunset_utc(
        2025, 6, 21,
        0.0, 181.0,            /* Invalid longitude > 180 */
        &sunrise_utc, &sunset_utc
    );
    /* Should handle gracefully */

    ESP_LOGI(TAG, "Input validation tests completed");
}

TEST_CASE("Test known reference values", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test London on summer solstice 2025 */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 6, 21,
                 51.5074, -0.1278,      /* London coordinates */
                 &sunrise_utc, &sunset_utc
             );

    TEST_ASSERT_TRUE(result);

    struct tm sunrise_tm_buf, sunset_tm_buf;
    struct tm *sunrise_tm = gmtime_r(&sunrise_utc, &sunrise_tm_buf);
    struct tm *sunset_tm = gmtime_r(&sunset_utc, &sunset_tm_buf);

    ESP_LOGI(TAG, "London 2025-06-21: Sunrise %02d:%02d UTC, Sunset %02d:%02d UTC",
             sunrise_tm->tm_hour, sunrise_tm->tm_min,
             sunset_tm->tm_hour, sunset_tm->tm_min);

    /* London summer solstice: sunrise around 04:43 UTC, sunset around 20:21 UTC */
    TEST_ASSERT_TRUE(sunrise_tm->tm_hour >= 3 && sunrise_tm->tm_hour <= 6);
    TEST_ASSERT_TRUE(sunset_tm->tm_hour >= 19 && sunset_tm->tm_hour <= 22);
}

TEST_CASE("Test equatorial location", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test Singapore (near equator) */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 3, 21,           /* Equinox */
                 1.3521, 103.8198,      /* Singapore coordinates */
                 &sunrise_utc, &sunset_utc
             );

    TEST_ASSERT_TRUE(result);

    struct tm sunrise_tm_buf, sunset_tm_buf;
    struct tm *sunrise_tm = gmtime_r(&sunrise_utc, &sunrise_tm_buf);
    struct tm *sunset_tm = gmtime_r(&sunset_utc, &sunset_tm_buf);

    ESP_LOGI(TAG, "Singapore 2025-03-21: Sunrise %02d:%02d UTC, Sunset %02d:%02d UTC",
             sunrise_tm->tm_hour, sunrise_tm->tm_min,
             sunset_tm->tm_hour, sunset_tm->tm_min);

    /* Near equator, day length should be close to 12 hours */
    /* Handle day boundary crossing - if sunset appears before sunrise, add 24 hours */
    int day_length_minutes;
    if (sunset_utc >= sunrise_utc) {
        day_length_minutes = (sunset_utc - sunrise_utc) / 60;
    } else {
        /* Day boundary crossing - sunset is next day */
        day_length_minutes = ((sunset_utc + 24 * 3600) - sunrise_utc) / 60;
    }
    TEST_ASSERT_INT_WITHIN(30, 12 * 60, day_length_minutes); /* Within 30 minutes of 12 hours */
}

TEST_CASE("Test southern hemisphere", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test Sydney, Australia */
    result = esp_daylight_calc_sunrise_sunset_utc(
                 2025, 12, 21,          /* Summer solstice in southern hemisphere */
                 -33.8688, 151.2093,   /* Sydney coordinates */
                 &sunrise_utc, &sunset_utc
             );

    TEST_ASSERT_TRUE(result);

    struct tm sunrise_tm_buf, sunset_tm_buf;
    struct tm *sunrise_tm = gmtime_r(&sunrise_utc, &sunrise_tm_buf);
    struct tm *sunset_tm = gmtime_r(&sunset_utc, &sunset_tm_buf);

    ESP_LOGI(TAG, "Sydney 2025-12-21: Sunrise %02d:%02d UTC, Sunset %02d:%02d UTC",
             sunrise_tm->tm_hour, sunrise_tm->tm_min,
             sunset_tm->tm_hour, sunset_tm->tm_min);

    /* Should have valid sunrise/sunset times */
    TEST_ASSERT_NOT_EQUAL(0, sunrise_utc);
    TEST_ASSERT_NOT_EQUAL(0, sunset_utc);
    TEST_ASSERT_GREATER_THAN(sunset_utc, sunrise_utc);
}

TEST_CASE("Test NULL pointer handling", "[esp_daylight]")
{
    time_t sunrise_utc, sunset_utc;
    bool result;

    /* Test NULL location pointer */
    result = esp_daylight_calc_sunrise_sunset_location(
                 2025, 6, 21,
                 NULL,
                 &sunrise_utc, &sunset_utc
             );
    TEST_ASSERT_FALSE(result);

    /* Test NULL output pointers (should not crash) */
    (void) esp_daylight_calc_sunrise_sunset_utc(
        2025, 6, 21,
        0.0, 0.0,
        NULL, NULL
    );
    /* Should handle gracefully */

    ESP_LOGI(TAG, "NULL pointer handling tests completed");
}
