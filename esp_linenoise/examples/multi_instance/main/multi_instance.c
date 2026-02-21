/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "common_io.h"
#include "esp_linenoise.h"
#include <stdio.h>
#include <string.h>

#define HISTORY_PATH_1 "history_instance1.txt"
#define HISTORY_PATH_2 "history_instance2.txt"
#define HISTORY_LEN 5

void app_main(void)
{
    common_init_io();

    // Create first instance for "user" commands
    esp_linenoise_config_t config1;
    esp_linenoise_get_instance_config_default(&config1);
    config1.prompt = "user> ";
    config1.history_max_length = HISTORY_LEN;

    esp_linenoise_handle_t handle1;
    esp_err_t err = esp_linenoise_create_instance(&config1, &handle1);
    if (err != ESP_OK) {
        printf("Failed to create first linenoise instance\n");
        return;
    }

    // Create second instance for "admin" commands
    esp_linenoise_config_t config2;
    esp_linenoise_get_instance_config_default(&config2);
    config2.prompt = "admin> ";
    config2.history_max_length = HISTORY_LEN;

    esp_linenoise_handle_t handle2;
    err = esp_linenoise_create_instance(&config2, &handle2);
    if (err != ESP_OK) {
        printf("Failed to create second linenoise instance\n");
        esp_linenoise_delete_instance(handle1);
        return;
    }

    char line[256];
    bool use_first = true;

    while (1) {
        esp_linenoise_handle_t current_handle = use_first ? handle1 : handle2;
        const char *mode = use_first ? "user" : "admin";

        printf("Current mode: %s\n", mode);
        err = esp_linenoise_get_line(current_handle, line, sizeof(line));
        if (err != ESP_OK) {
            break;
        }

        if (strlen(line) > 0) {
            if (strcmp(line, "exit") == 0) {
                break;
            } else if (strcmp(line, "switch") == 0) {
                use_first = !use_first;
                printf("Switched to %s mode\n\n", use_first ? "user" : "admin");
            } else {
                printf("[%s] You entered: %s\n", mode, line);
                esp_linenoise_history_add(current_handle, line);
            }
        }

        memset(line, 0, sizeof(line));
    }

    // Save separate histories
    esp_linenoise_history_save(handle1, HISTORY_PATH_1);
    esp_linenoise_history_save(handle2, HISTORY_PATH_2);

    // Cleanup
    esp_linenoise_delete_instance(handle1);
    esp_linenoise_delete_instance(handle2);
    common_deinit_io();

    printf("end of example\n");
}
