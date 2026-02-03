/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_linenoise.h"
#include "common_io.h"

void app_main(void)
{
    common_init_io();

    esp_linenoise_config_t config;
    esp_linenoise_handle_t handle;
    esp_linenoise_get_instance_config_default(&config);
    config.prompt = "esp_linenoise> ";
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&config, &handle));

    bool dumb_mode = false;
    esp_linenoise_is_dumb_mode(handle, &dumb_mode);
    if (dumb_mode) {
        printf("Running in dumb mode\n");
    } else {
        printf("Running in normal mode\n");
    }

    char buffer[128];
    const esp_err_t ret_val = esp_linenoise_get_line(handle, buffer, sizeof(buffer));
    if (ret_val == ESP_OK) {
        printf("You entered: %s\n", buffer);
    } else {
        printf("No input received\n");
    }

    ESP_ERROR_CHECK(esp_linenoise_delete_instance(handle));
    common_deinit_io();

    printf("end of example\n");
}
