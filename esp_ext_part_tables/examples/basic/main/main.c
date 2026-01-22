/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_ext_part_tables.h"
#include "esp_mbr.h"
#include "esp_mbr_utils.h"

#include "example_utils.h"

static const char *TAG = "esp_ext_part_tables_example_basic";

void print_loaded_ext_partitions(esp_ext_part_list_item_t *head)
{
    esp_ext_part_list_item_t *it = head;
    int i = 0;
    do {
        printf("Partition %d:\n\tLBA start sector: %" PRIu64 ", address: %" PRIu64 ",\n\tsector count: %" PRIu64 ", size: %" PRIu64 ",\n\ttype: %s\n\n",
               i,
               esp_ext_part_bytes_to_sector_count(it->info.address, ESP_EXT_PART_SECTOR_SIZE_512B), it->info.address,
               esp_ext_part_bytes_to_sector_count(it->info.size, ESP_EXT_PART_SECTOR_SIZE_512B), it->info.size,
               parsed_type_to_str(it->info.type));
        i++;
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
    fflush(stdout);
}

void esp_ext_part_tables_mbr_parse_example_task(void *pvParameters)
{
    TaskHandle_t main_task_handle;
    ESP_LOGI(TAG, "Starting MBR parsing example task");
    esp_err_t err;

    // Allocate memory for the MBR
    mbr_t *mbr = (mbr_t *) heap_caps_malloc(sizeof(mbr_t), (MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (mbr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for MBR");
        goto end_task;
    }

    // Load the first sector (MBR) from the SD card into the allocated buffer
    err = load_first_sector_from_sd_card(mbr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load MBR from SD card: %s", esp_err_to_name(err));
        free(mbr);
        goto end_task;
    }
    ESP_LOGI(TAG, "MBR loaded successfully");

    // Parse the MBR to get the partition list
    esp_ext_part_list_t part_list = {0};
    err = esp_mbr_parse((void *) mbr, &part_list, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse MBR: %s", esp_err_to_name(err));
        free(mbr);
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }
    free(mbr); // Free the MBR buffer after parsing as it is no longer needed
    ESP_LOGI(TAG, "MBR parsed successfully");

    // Get the first partition
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list);
    if (it == NULL) {
        ESP_LOGE(TAG, "No partitions found in the MBR");
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }

    // Print the loaded partition list
    print_loaded_ext_partitions(it);

    // Deinitialize the partition list
    esp_ext_part_list_deinit(&part_list);

    ESP_LOGI(TAG, "MBR parsing example task completed successfully");

    // Notify the main task that the example is done and end the current task
end_task:
    main_task_handle = (TaskHandle_t) pvParameters;
    xTaskNotifyGive(main_task_handle);
    vTaskDelete(NULL); // Delete the current task
}

void esp_ext_part_tables_mbr_generate_example_task(void *pvParameters)
{
    TaskHandle_t main_task_handle;
    ESP_LOGI(TAG, "Starting MBR generation example task");
    esp_err_t err;
    esp_ext_part_list_t part_list = {0};

    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    // 2 FAT12 partitions (random parameters)
    esp_ext_part_list_item_t item1 = {
        .info = {
            // Original MBR starts at 2048, but we use 8 for testing ->
            .address = esp_ext_part_sector_count_to_bytes(8, mbr_args.sector_size), // Should be round up to 2048 sectors (aligned to 1MiB) due to defined sector size and alignment in `esp_mbr_generate_extra_args_t args` above
            .size = esp_ext_part_sector_count_to_bytes(7953, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .label = NULL,
        }
    };
    err = esp_ext_part_list_insert(&part_list, &item1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to insert first partition: %s", esp_err_to_name(err));
        goto end_task;
    }

    esp_ext_part_list_item_t item2 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(10240, mbr_args.sector_size),
            .size = esp_ext_part_sector_count_to_bytes(10240, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .label = NULL,
        }
    };
    esp_ext_part_list_insert(&part_list, &item2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to insert second partition: %s", esp_err_to_name(err));
        goto end_task;
    }

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    if (mbr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for MBR");
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }
    // Generate the MBR
    err = esp_mbr_generate(mbr, &part_list, &mbr_args);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate MBR: %s", esp_err_to_name(err));
        free(mbr);
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }
    ESP_LOGI(TAG, "MBR generated successfully");

    // Deinitialize the partition list
    esp_ext_part_list_deinit(&part_list);

    esp_ext_part_list_t part_list_from_gen_mbr = {0};
    // Parse the generated MBR to get the partition list
    err = esp_mbr_parse((void *) mbr, &part_list_from_gen_mbr, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse generated MBR: %s", esp_err_to_name(err));
        free(mbr);
        goto end_task;
    }
    free(mbr); // Free the MBR buffer after parsing as it is no longer needed

    // Get the first partition
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list_from_gen_mbr);
    if (it == NULL) {
        ESP_LOGE(TAG, "No partitions found in the MBR");
        esp_ext_part_list_deinit(&part_list_from_gen_mbr);
        goto end_task;
    }

    // Print the loaded partition list
    print_loaded_ext_partitions(it);

    // Deinitialize the partition list
    esp_ext_part_list_deinit(&part_list_from_gen_mbr);

    ESP_LOGI(TAG, "MBR generation example task completed successfully");

    // Notify the main task that the example is done and end the current task
end_task:
    main_task_handle = (TaskHandle_t) pvParameters;
    xTaskNotifyGive(main_task_handle);
    vTaskDelete(NULL); // Delete the current task
}

void app_main(void)
{
    ESP_LOGI(TAG, "Example started");
    TaskHandle_t main_task_handle = xTaskGetCurrentTaskHandle(); // Get the handle of the main task

    // Create new tasks with bigger stack size just in case
    xTaskCreate(esp_ext_part_tables_mbr_parse_example_task, "esp_ext_part_tables_mbr_parse_example_task", 4096, (void *) main_task_handle, 5, NULL);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for the example task to complete

    xTaskCreate(esp_ext_part_tables_mbr_generate_example_task, "esp_ext_part_tables_mbr_generate_example_task", 4096, (void *) main_task_handle, 5, NULL);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for the example task to complete

    ESP_LOGI(TAG, "Example ended");
}
