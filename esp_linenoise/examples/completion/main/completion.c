/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "common_io.h"
#include "esp_linenoise.h"
#include <stdio.h>
#include <string.h>

// List of commands for completion
static const char *commands[] = {
    "help",
    "history",
    "clear",
    "exit",
    "status",
    "config",
    "reset",
    NULL
};

// Completion callback function
static void completion_callback(const char *buf, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    for (int i = 0; commands[i] != NULL; i++) {
        if (strncmp(buf, commands[i], strlen(buf)) == 0) {
            cb(cb_ctx, commands[i]);
        }
    }
}

void app_main(void)
{
    common_init_io();

    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);
    config.prompt = "completion> ";
    config.completion_cb = completion_callback;

    esp_linenoise_handle_t handle;
    esp_err_t err = esp_linenoise_create_instance(&config, &handle);
    if (err != ESP_OK) {
        printf("Failed to create linenoise instance\n");
        return;
    }

    printf("Tab completion example. Try typing 'h' and press TAB.\n");
    printf("Available commands: help, history, clear, exit, status, config, reset\n");

    char line[256];
    while (1) {
        err = esp_linenoise_get_line(handle, line, sizeof(line));
        if (err != ESP_OK) {
            break;
        }

        if (strlen(line) > 0) {
            printf("You entered: %s\n", line);
            esp_linenoise_history_add(handle, line);

            if (strcmp(line, "exit") == 0) {
                break;
            }
        }
    }

    esp_linenoise_delete_instance(handle);
    common_deinit_io();

    printf("end of example\n");
}
