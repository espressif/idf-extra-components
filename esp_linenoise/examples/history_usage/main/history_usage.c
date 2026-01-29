/*
 * esp_linenoise history usage example
 * Demonstrates enabling, using, and persisting input history.
 */
#include "esp_linenoise.h"
#include <stdio.h>
#include <stdlib.h>

#define HISTORY_PATH "linenoise_history.txt"
#define HISTORY_LEN 10

void app_main(void)
{
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
