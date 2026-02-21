/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_linenoise.h"
#include <stdio.h>
#include <stdlib.h>

#if CONFIG_IDF_TARGET_LINUX
#define HISTORY_PATH "linenoise_history.txt"
#else
#include "esp_spiffs.h"
#define STORAGE_MOUNT_POINT "/storage"
#define HISTORY_PATH STORAGE_MOUNT_POINT "/linenoise_history.txt"
#endif

#define HISTORY_LEN 10

#if !CONFIG_IDF_TARGET_LINUX
static void init_filesystem(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = STORAGE_MOUNT_POINT,
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount or format filesystem\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("Failed to find SPIFFS partition\n");
        } else {
            printf("Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        printf("SPIFFS partition size: total: %d, used: %d\n", total, used);
    }
}
#endif

void app_main(void)
{
#if !CONFIG_IDF_TARGET_LINUX
    init_filesystem();
#endif

    esp_linenoise_config_t config;
    esp_linenoise_handle_t handle;
    esp_linenoise_get_instance_config_default(&config);
    config.prompt = "esp_linenoise> ";
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&config, &handle));

    /* create a fake history saved in filename_history */
    FILE *fp = fopen(HISTORY_PATH, "w");
    if (fp == NULL) {
        printf("Failed to create history file\n");
        return;
    }
    fputs("first command line\n", fp);
    fputs("second command line\n", fp);
    fclose(fp);

    ESP_ERROR_CHECK(esp_linenoise_history_set_max_len(handle, HISTORY_LEN));
    ESP_ERROR_CHECK(esp_linenoise_history_load(handle,  HISTORY_PATH));

    ESP_ERROR_CHECK(esp_linenoise_history_add(handle, "random command line 1"));
    ESP_ERROR_CHECK(esp_linenoise_history_add(handle, "random command line 2"));
    ESP_ERROR_CHECK(esp_linenoise_history_save(handle, HISTORY_PATH));

    fp = fopen(HISTORY_PATH, "r");
    if (fp == NULL) {
        printf("Failed to create history file\n");
        return;
    }

    char buffer[64];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("History entry: %s", buffer);
    }
    fclose(fp);

    printf("end of example\n");
}
