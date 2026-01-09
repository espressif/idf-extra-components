/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_schedule.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "lwip/ip_addr.h"
#include "network_provisioning/manager.h"
#include "freertos/event_groups.h"
#include "app_network.h"

static const char *TAG = "esp_schedule_example";

/* Event group for network provisioning synchronization */
static EventGroupHandle_t s_network_event_group;

// Callback functions for different schedule types
static void days_of_week_callback(esp_schedule_handle_t handle, void *priv_data)
{
    ESP_LOGI(TAG, "Days of week schedule triggered! Data: %s", (char *)priv_data);

    // Example: Toggle an LED, send a notification, etc.
    // Your application logic here
}

static void date_callback(esp_schedule_handle_t handle, void *priv_data)
{
    ESP_LOGI(TAG, "Date schedule triggered! Data: %s", (char *)priv_data);

    // Example: Birthday reminder, anniversary notification, etc.
    // Your application logic here
}

static void relative_callback(esp_schedule_handle_t handle, void *priv_data)
{
    ESP_LOGI(TAG, "Relative schedule triggered! Data: %s", (char *)priv_data);

    // Example: Timer expired, delayed action completed, etc.
    // Your application logic here
}

static void multi_callback(esp_schedule_handle_t handle, void *priv_data)
{
    ESP_LOGI(TAG, "Multi-trigger schedule triggered! Data: %s", (char *)priv_data);

    // Example: Multiple triggers occurred, etc.
    // Your application logic here
}

#ifdef CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
static void solar_callback(esp_schedule_handle_t handle, void *priv_data)
{
    ESP_LOGI(TAG, "Solar schedule triggered! Data: %s", (char *)priv_data);

    // Example: Turn on lights at sunset, turn off lights at sunrise, etc.
    // Your application logic here
}
#endif // CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT

static void timestamp_callback(esp_schedule_handle_t handle, uint32_t next_timestamp, void *priv_data)
{
    time_t timestamp = (time_t)next_timestamp;
    char *time_str = ctime(&timestamp);
    if (time_str) {
        ESP_LOGI(TAG, "Next schedule timestamp updated for %s: %s", (char *)priv_data, time_str);
    } else {
        ESP_LOGI(TAG, "Next schedule timestamp updated for %s: <invalid time>", (char *)priv_data);
    }
}

// Private data for different schedules
static char *days_of_week_data = "Monday/Wednesday/Friday schedule";
static char *date_data = "Monthly schedule";
static char *relative_data = "Timer schedule";
static char *multi_data = "Multi-trigger schedule";
#ifdef CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
static char *solar_data = "Sunrise/Sunset schedule";
#endif // CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT

/**
 * @brief Create example schedules
 *
 * This function creates the example schedules using the esp_schedule_create() API.
 * @note If NVS is enabled, then the names of the schedules are used as NVS keys, so they must be shorter than the NVS key length limit of 16 characters.
 */
static void create_example_schedules(void)
{
    ESP_LOGI(TAG, "Creating example schedules...");
    esp_schedule_handle_t handle;
    esp_err_t ret;

    // Example 1: Days of week schedule
    // Triggers every Monday, Wednesday, and Friday at 14:30
    esp_schedule_trigger_t days_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
            .hours = 14,
            .minutes = 30,
            .day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_FRIDAY
        }
    };
    esp_schedule_config_t days_schedule = {
        .name = "work_days",
        .triggers = {
            .list = days_trigger_list,
            .count = sizeof(days_trigger_list) / sizeof(days_trigger_list[0])
        },
        .trigger_cb = days_of_week_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = days_of_week_data,
        .validity = {
            .start_time = 0,  // Start immediately
            .end_time = 0     // No end time (run indefinitely)
        }
    };

    ret = esp_schedule_create(&days_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create days of week schedule");
        return;
    }
    ESP_LOGI(TAG, "Created days of week schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable days of week schedule");
        return;
    }

    // Example 2: Date schedule
    // Triggers every month on the 15th at 09:00
    esp_schedule_trigger_t date_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_DATE,
            .hours = 9,
            .minutes = 0,
            .date.day = 15,
            .date.repeat_months = ESP_SCHEDULE_MONTH_ALL,
            .date.repeat_every_year = true
        }
    };
    esp_schedule_config_t date_schedule = {
        .name = "monthly_15",
        .triggers = {
            .list = date_trigger_list,
            .count = sizeof(date_trigger_list) / sizeof(date_trigger_list[0])
        },
        .trigger_cb = date_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = date_data,
        .validity = {
            .start_time = 0,  // Start immediately
            .end_time = 0     // No end time
        }
    };

    ret = esp_schedule_create(&date_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create date schedule");
        return;
    }
    ESP_LOGI(TAG, "Created date schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable date schedule");
        return;
    }

    // Example 3: Relative schedule
    // Triggers after 10 seconds from creation
    time_t current_time = time(NULL);
    esp_schedule_trigger_t relative_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_RELATIVE,
            .relative_seconds = 10,  // 10 seconds from now
        }
    };
    esp_schedule_config_t relative_schedule = {
        .name = "10_sec",
        .triggers = {
            .list = relative_trigger_list,
            .count = sizeof(relative_trigger_list) / sizeof(relative_trigger_list[0])
        },
        .trigger_cb = relative_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = relative_data,
        .validity = {
            .start_time = current_time,
            .end_time = current_time + 120  // Valid for 2 minutes
        }
    };

    ret = esp_schedule_create(&relative_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create relative schedule");
        return;
    }
    ESP_LOGI(TAG, "Created relative schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable relative schedule");
        return;
    }

    // Example 4: Multi-trigger schedule
    // Triggers every Monday, Wednesday, and Friday at 14:30, and every month on the 15th at 09:00
    esp_schedule_trigger_t multi_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
            .hours = 14,
            .minutes = 30,
            .day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_FRIDAY
        },
        {
            .type = ESP_SCHEDULE_TYPE_DATE,
            .hours = 9,
            .minutes = 0,
            .date.day = 15,
            .date.repeat_months = ESP_SCHEDULE_MONTH_ALL,
            .date.repeat_every_year = true
        }
    };
    esp_schedule_config_t multi_schedule = {
        .name = "multi",
        .triggers = {
            .list = multi_trigger_list,
            .count = sizeof(multi_trigger_list) / sizeof(multi_trigger_list[0])
        },
        .trigger_cb = multi_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = multi_data,
        .validity = {
            .start_time = 0,  // Start immediately
            .end_time = 0     // No end time
        }
    };

    ret = esp_schedule_create(&multi_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create multi-trigger schedule");
        return;
    }
    ESP_LOGI(TAG, "Created multi-trigger schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable multi-trigger schedule");
        return;
    }

    // Example 5: Solar schedule (Sunrise/Sunset) with day-of-week filtering
    // Note: This requires CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT to be enabled
    // Triggers at sunrise and sunset for a specific location, but only on weekdays
#ifdef CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    esp_schedule_trigger_t sunrise_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_SUNRISE,
            .hours = 0,  // Hours/minutes are ignored for solar schedules
            .minutes = 0,
            .day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_TUESDAY |
            ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_THURSDAY | ESP_SCHEDULE_DAY_FRIDAY,
            .solar.latitude = 37.7749,    // San Francisco latitude
            .solar.longitude = -122.4194, // San Francisco longitude
            .solar.offset_minutes = 0,    // Exactly at sunrise
        }
    };
    esp_schedule_config_t sunrise_schedule = {
        .name = "sunrise",
        .triggers = {
            .list = sunrise_trigger_list,
            .count = sizeof(sunrise_trigger_list) / sizeof(sunrise_trigger_list[0])
        },
        .trigger_cb = solar_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = solar_data,
        .validity = {
            .start_time = 0,  // Start immediately
            .end_time = 0     // No end time
        }
    };

    ret = esp_schedule_create(&sunrise_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create sunrise schedule");
        return;
    }
    ESP_LOGI(TAG, "Created sunrise schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable sunrise schedule");
        return;
    }

    esp_schedule_trigger_t sunset_trigger_list[] = {
        {
            .type = ESP_SCHEDULE_TYPE_SUNSET,
            .hours = 0,  // Hours/minutes are ignored for solar schedules
            .minutes = 0,
            .day.repeat_days = ESP_SCHEDULE_DAY_MONDAY | ESP_SCHEDULE_DAY_TUESDAY |
            ESP_SCHEDULE_DAY_WEDNESDAY | ESP_SCHEDULE_DAY_THURSDAY | ESP_SCHEDULE_DAY_FRIDAY,
            .solar.latitude = 37.7749,    // San Francisco latitude
            .solar.longitude = -122.4194, // San Francisco longitude
            .solar.offset_minutes = -30,  // 30 minutes before sunset
        }
    };
    esp_schedule_config_t sunset_schedule = {
        .name = "sunset",
        .triggers = {
            .list = sunset_trigger_list,
            .count = sizeof(sunset_trigger_list) / sizeof(sunset_trigger_list[0])
        },
        .trigger_cb = solar_callback,
        .timestamp_cb = timestamp_callback,
        .priv_data = solar_data,
        .validity = {
            .start_time = 0,  // Start immediately
            .end_time = 0     // No end time
        }
    };

    ret = esp_schedule_create(&sunset_schedule, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create sunset schedule");
        return;
    }
    ESP_LOGI(TAG, "Created sunset schedule successfully");
    ret = esp_schedule_enable(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable sunset schedule");
        return;
    }
#endif // CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
}

void app_main(void)
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Event Loop (Network Interface is initialized in app_network)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create event group for network provisioning synchronization
    s_network_event_group = xEventGroupCreate();
    if (s_network_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Initialize network provisioning (includes WiFi and network interfaces)
    ESP_ERROR_CHECK(app_network_init(s_network_event_group));

    // Start network provisioning and wait for connection
    esp_err_t network_result = app_network_start(s_network_event_group, 300000); // 5 minutes timeout
    if (network_result != ESP_OK) {
        ESP_LOGE(TAG, "Network connection failed or timed out");
        return;
    }

    // Start time synchronization
    app_network_start_time_sync(s_network_event_group);

    // Wait for time synchronization to complete
    esp_err_t time_result = app_network_wait_for_time_sync(s_network_event_group, 60000); // 1 minute timeout
    if (time_result != ESP_OK) {
        ESP_LOGW(TAG, "Time synchronization failed or timed out, continuing anyway");
    }

    // Initialize ESP Schedule
    ESP_LOGI(TAG, "Initializing ESP Schedule...");
#if CONFIG_ESP_SCHEDULE_ENABLE_NVS
    uint8_t schedule_count;
    esp_schedule_handle_t *schedule_list = NULL;
    ESP_ERROR_CHECK(esp_schedule_init_nvs(NULL, NULL, &schedule_count, &schedule_list));
    if (schedule_list != NULL) {
        // If there are existing schedules in NVS, their handles will be available in this list. We don't use them in this example, so we free the array.
        free(schedule_list);
    }
#else
    ESP_ERROR_CHECK(esp_schedule_init_default());
#endif

    // Make all the schedules used
    create_example_schedules();

    ESP_LOGI(TAG, "ESP Schedule example started. Schedules will trigger based on their configurations.");

    // The schedules will now trigger automatically based on their configurations
    // Monitor network status and handle disconnections
    while (1) {
        // Main application loop
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second

        // Print current time every 10 seconds for reference
        static int counter = 0;
        if (++counter >= 10) {
            time_t now = time(NULL);
            char *time_str = ctime(&now);
            if (time_str) {
                ESP_LOGI(TAG, "Current time: %s", time_str);
            } else {
                ESP_LOGI(TAG, "Current time: <invalid>");
            }
            counter = 0;
        }

        // Check for network disconnection and handle it
        EventBits_t network_bits = xEventGroupGetBits(s_network_event_group);
        if (network_bits & NETWORK_DISCONNECTED_BIT) {
            ESP_LOGW(TAG, "Network disconnected, attempting to reconnect...");
            // For now, just log the issue - in a real application you might
            // want to restart provisioning or handle reconnection
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before checking again
        }
    }

    // Cleanup (this won't be reached in the current infinite loop, but good practice)
    if (s_network_event_group) {
        vEventGroupDelete(s_network_event_group);
    }
}
