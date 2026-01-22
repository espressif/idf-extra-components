/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <unity.h>
#include "esp_flash.h"
#include <esp_attr.h>
#include "esp_log.h"
#include "unity.h"
#include "sdkconfig.h"
#include "esp_flash_dispatcher.h"
#include "esp_partition.h"

// Buffer for PSRAM task stack
static StackType_t *s_psram_task_stack = NULL;

#define TEST_FLASH_ADDRESS 0x110000 // default address in flash for testing
#define TEST_FLASH_SIZE 4096
#define TEST_PSRAM_TASK_STACK_SIZE 4096

static const esp_partition_t *get_test_data_partition(void)
{
    /* This finds "flash_test" partition defined in partition_table_unit_test_app.csv */
    const esp_partition_t *result = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_ANY, "flash_test");
    TEST_ASSERT_NOT_NULL(result); /* means partition table set wrong */
    return result;
}

// The PSRAM task will take the creating task's handle as argument to notify it upon completion
static void psram_flash_test_task(void *arg)
{
    const esp_partition_t *test_part = get_test_data_partition();
    TaskHandle_t creating_task_handle = (TaskHandle_t)arg;

    ESP_LOGI("PSRAM_TASK", "PSRAM task started on core %d", xPortGetCoreID());
    esp_err_t ret;

    uint8_t *write_buf = (uint8_t *)heap_caps_malloc(TEST_FLASH_SIZE, MALLOC_CAP_INTERNAL);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(TEST_FLASH_SIZE, MALLOC_CAP_INTERNAL);
    TEST_ASSERT_NOT_NULL(write_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    // Fill write buffer with some pattern
    for (int i = 0; i < TEST_FLASH_SIZE; i++) {
        write_buf[i] = (uint8_t)(i % 256);
    }
    ESP_LOGI("PSRAM_TASK", "Performing flash erase at 0x%lx, size 0x%x", test_part->address, TEST_FLASH_SIZE);
    ret = esp_flash_erase_region(NULL, test_part->address, 4096);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI("PSRAM_TASK", "Flash erase successful.");

    ESP_LOGI("PSRAM_TASK", "Performing flash write to 0x%lx, size 0x%x", test_part->address, TEST_FLASH_SIZE);
    ret = esp_flash_write(NULL, write_buf, test_part->address, TEST_FLASH_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI("PSRAM_TASK", "Flash write successful.");

    ESP_LOGI("PSRAM_TASK", "Performing flash read from 0x%lx, size 0x%x", test_part->address, TEST_FLASH_SIZE);
    ret = esp_flash_read(NULL, read_buf, test_part->address, TEST_FLASH_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI("PSRAM_TASK", "Flash read successful.");

    // Verify data
    TEST_ASSERT_EQUAL_HEX8_ARRAY(write_buf, read_buf, TEST_FLASH_SIZE);
    ESP_LOGI("PSRAM_TASK", "Flash read data verified successfully.");

    heap_caps_free(write_buf);
    heap_caps_free(read_buf);

    // Notify the creating task that this task has completed
    xTaskNotifyGive(creating_task_handle);

    vTaskDelete(NULL);
}

TEST_CASE("Flash operations from PSRAM task", "[flash_dispatcher]")
{
    // Use default configuration when cfg is NULL
    const esp_flash_dispatcher_config_t cfg = ESP_FLASH_DISPATCHER_DEFAULT_CONFIG;
    esp_flash_dispatcher_init(&cfg);

    ESP_LOGI("TEST", "Creating PSRAM task");
    s_psram_task_stack = (StackType_t *)heap_caps_malloc(TEST_PSRAM_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    TEST_ASSERT_NOT_NULL(s_psram_task_stack);

    TaskHandle_t psram_task_handle;
    xTaskCreatePinnedToCore(
        psram_flash_test_task,
        "psram_flash_test",
        TEST_PSRAM_TASK_STACK_SIZE, // The size here is in bytes for dynamic allocation
        (void *)xTaskGetCurrentTaskHandle(), // Pass the current task handle as argument
        5, // Priority
        &psram_task_handle,
        0 // Run on APP_CPU if available, otherwise PRO_CPU
    );
    TEST_ASSERT_NOT_NULL(psram_task_handle);

    // Wait for the PSRAM task to complete
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    vTaskDelay(2);

    heap_caps_free(s_psram_task_stack);
    s_psram_task_stack = NULL;
}
