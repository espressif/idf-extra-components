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

// A fixed disk size used for the generation demo so that FILL and the disk-bounds
// check have a known total to work with (16 MiB).
#define EXAMPLE_DISK_SIZE_BYTES (16 * 1024 * 1024)

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

// List only the partitions this build can mount, using the stock mountable predicate.
static void print_mountable_partitions(esp_ext_part_list_t *part_list)
{
    ESP_LOGI(TAG, "Mountable partitions (via esp_ext_part_list_next_matching):");
    int count = 0;
    esp_ext_part_match_t matcher = esp_ext_part_match_mountable();
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, part_list, &matcher);
            it != NULL;
            it = esp_ext_part_list_next_matching(it, part_list, &matcher)) {
        printf("\t- %s at sector %" PRIu64 "\n",
               parsed_type_to_str(it->info.type),
               esp_ext_part_bytes_to_sector_count(it->info.address, ESP_EXT_PART_SECTOR_SIZE_512B));
        count++;
    }
    if (count == 0) {
        printf("\t(none)\n");
    }
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

    // Parse the MBR to get the partition list. With extra_args == NULL, every
    // recognized partition is inserted (mountable or not).
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

    // If the parser skipped any partition (unknown/extended type), the list is
    // marked LOSSY - regenerating it would not reproduce the source table.
    if (part_list.flags & ESP_EXT_PART_LIST_FLAG_LOSSY) {
        ESP_LOGW(TAG, "Parsed list is LOSSY: some source partitions were not captured");
    } else {
        ESP_LOGI(TAG, "Parsed list is complete (not LOSSY)");
    }

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

    // A zero-initialized args struct already selects sensible defaults
    // (AUTO alignment => 1 MiB, KEEP_SIZE policy, 512 B sectors). Here we set only
    // what the auto-placement/FILL demo needs: an explicit sector size and the
    // total disk size (so FILL and the bounds check know where the disk ends).
    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .total_size = EXAMPLE_DISK_SIZE_BYTES,
        // .alignment left 0 (ESP_EXT_PART_ALIGN_AUTO) -> resolves to 1 MiB
    };

    // Partition 1: auto-placed (library picks the aligned start), fixed 4 MiB FAT32.
    esp_ext_part_list_item_t item1 = {
        .info = {
            .size = 4 * 1024 * 1024,
            .type = ESP_EXT_PART_TYPE_FAT32,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    err = esp_ext_part_list_insert(&part_list, &item1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to insert first partition: %s", esp_err_to_name(err));
        goto end_task;
    }

    // Partition 2: auto-placed raw-data (0xDA) partition, fixed 2 MiB. Demonstrates
    // a non-filesystem partition the application manages itself.
    esp_ext_part_list_item_t item2 = {
        .info = {
            .size = 2 * 1024 * 1024,
            .type = ESP_EXT_PART_TYPE_RAW_DATA,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    err = esp_ext_part_list_insert(&part_list, &item2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to insert second partition: %s", esp_err_to_name(err));
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }

    // Partition 3: auto-placed and FILL - sized to consume the rest of the disk.
    esp_ext_part_list_item_t item3 = {
        .info = {
            .size = 0, // filled to the end of the disk
            .type = ESP_EXT_PART_TYPE_FAT16,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS | ESP_EXT_PART_FLAG_FILL,
        }
    };
    err = esp_ext_part_list_insert(&part_list, &item3);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to insert third partition: %s", esp_err_to_name(err));
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    if (mbr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for MBR");
        esp_ext_part_list_deinit(&part_list);
        goto end_task;
    }
    // Generate the MBR. Addresses/sizes for the AUTO_ADDRESS/FILL items are
    // resolved internally; the caller's list is not modified.
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

    // Print the loaded partition list, then list only the mountable ones.
    print_loaded_ext_partitions(it);
    print_mountable_partitions(&part_list_from_gen_mbr);

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
