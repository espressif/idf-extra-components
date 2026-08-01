/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file log.c
 * @brief esp_log implementation of esp_schedule_log_fn_t.
 *
 * @note Reached only through esp_schedule_esp_log, which only
 *       esp_schedule_init() references. A build that installs its own port
 *       never links this file, and with it never pulls in esp_log.
 */

#include "esp_schedule.h"
#include "esp_schedule_esp_port.h"

#include "esp_log.h"

/* The message arrives already formatted, so this only has to pick the matching
 * ESP_LOGx macro. Logging it as a single "%s" keeps the standard
 * "E (123) tag:" banner without reaching for esp_log internals, whose shape has
 * changed across IDF versions. */
void esp_schedule_esp_log(esp_schedule_log_level_t level, const char *tag, const char *message)
{
    switch (level) {
    case ESP_SCHEDULE_LOG_ERROR:
        ESP_LOGE(tag, "%s", message);
        break;
    case ESP_SCHEDULE_LOG_WARN:
        ESP_LOGW(tag, "%s", message);
        break;
    case ESP_SCHEDULE_LOG_INFO:
        ESP_LOGI(tag, "%s", message);
        break;
    case ESP_SCHEDULE_LOG_DEBUG:
        ESP_LOGD(tag, "%s", message);
        break;
    default:
        ESP_LOGV(tag, "%s", message);
        break;
    }
}
