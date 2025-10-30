/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *supported_targets[] = {
    "esp32",
    "esp32s2",
    "esp32s3",
    "esp32c2",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32c61",
};

void app_main(void)
{
    // This is compiled only if CONFIG_IDF_TARGET is not in the list of supported targets.
    char supported_targets_str[256] = "";
    for (int i = 0; i < sizeof(supported_targets) / sizeof(supported_targets[0]); i++) {
        char target_str[32];
        snprintf(target_str, sizeof(target_str), "\n\t%s", supported_targets[i]);
        strcat(supported_targets_str, target_str);
    }
    ESP_LOGE("esp_schedule_example", "Target '%s' is not supported. Please try one of these supported targets:%s", CONFIG_IDF_TARGET, supported_targets_str);
    return;
}