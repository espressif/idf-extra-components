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
#include "esp_idf_version.h"
#include "esp_flash_dispatcher.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"

// Buffer for PSRAM task stack
static StackType_t *s_psram_task_stack = NULL;

#define TEST_FLASH_ADDRESS 0x110000 // default address in flash for testing
#define TEST_FLASH_SIZE 4096
#define TEST_PSRAM_TASK_STACK_SIZE 4096

static void init_flash_dispatcher(void)
{
    static bool initialized;
    if (initialized) {
        return;
    }
    const esp_flash_dispatcher_config_t cfg = ESP_FLASH_DISPATCHER_DEFAULT_CONFIG;
    TEST_ASSERT_EQUAL(ESP_OK, esp_flash_dispatcher_init(&cfg));
    initialized = true;
}

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

    uint8_t *write_buf = (uint8_t *)heap_caps_malloc(TEST_FLASH_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(TEST_FLASH_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
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

    // mmap test
    const void *mmap_ptr = NULL;
    spi_flash_mmap_handle_t mmap_handle = 0;
    ESP_LOGI("PSRAM_TASK", "Performing flash mmap at 0x%lx, size 0x%x", test_part->address, TEST_FLASH_SIZE);
    ret = spi_flash_mmap(test_part->address, TEST_FLASH_SIZE, SPI_FLASH_MMAP_DATA, &mmap_ptr, &mmap_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(mmap_ptr);
    TEST_ASSERT_NOT_EQUAL(0, mmap_handle);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(write_buf, mmap_ptr, TEST_FLASH_SIZE);
    ESP_LOGI("PSRAM_TASK", "Flash mmap data verified successfully.");

    spi_flash_munmap(mmap_handle);
    ESP_LOGI("PSRAM_TASK", "Flash munmap successful.");

    heap_caps_free(write_buf);
    heap_caps_free(read_buf);

    // Notify the creating task that this task has completed
    xTaskNotifyGive(creating_task_handle);

    vTaskDelete(NULL);
}

TEST_CASE("Flash operations from PSRAM task", "[flash_dispatcher]")
{
    init_flash_dispatcher();

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

#if !CONFIG_IDF_TARGET_ESP32 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
typedef struct {
    SemaphoreHandle_t mmap_held;
    SemaphoreHandle_t allow_unmap;
    SemaphoreHandle_t writer_started;
    SemaphoreHandle_t holder_done;
    SemaphoreHandle_t writer_done;
    esp_err_t mmap_result;
    esp_err_t writer_result;
    uint8_t write_data[16];
} mmap_deadlock_test_ctx_t;

// Callers ensure the task can only block inside the flash write.
static void wait_task_blocked(TaskHandle_t task)
{
    TickType_t start = xTaskGetTickCount();
    while (eTaskGetState(task) != eBlocked) {
        TEST_ASSERT_LESS_THAN_UINT32(pdMS_TO_TICKS(2000), xTaskGetTickCount() - start);
        vTaskDelay(1);
    }
}

static void blocking_mmap_holder_task(void *arg)
{
    mmap_deadlock_test_ctx_t *ctx = arg;
    const esp_partition_t *partition = get_test_data_partition();
    const void *mmap_ptr = NULL;
    esp_partition_mmap_handle_t mmap_handle = 0;

    ctx->mmap_result = esp_partition_mmap(partition, 0, TEST_FLASH_SIZE,
                                          ESP_PARTITION_MMAP_DATA | ESP_PARTITION_MMAP_BLOCKS_WRITE,
                                          &mmap_ptr, &mmap_handle);
    if (ctx->mmap_result == ESP_OK) {
        xSemaphoreGive(ctx->mmap_held);
        xSemaphoreTake(ctx->allow_unmap, portMAX_DELAY);
        esp_partition_munmap(mmap_handle);
    }
    xSemaphoreGive(ctx->holder_done);
    vTaskSuspend(NULL);
}

static void flash_writer_task(void *arg)
{
    mmap_deadlock_test_ctx_t *ctx = arg;
    const esp_partition_t *partition = get_test_data_partition();

    xSemaphoreTake(ctx->mmap_held, portMAX_DELAY);
    xSemaphoreGive(ctx->writer_started);
    ctx->writer_result = esp_partition_write(partition, TEST_FLASH_SIZE,
                                             ctx->write_data, sizeof(ctx->write_data));
    xSemaphoreGive(ctx->writer_done);
    vTaskSuspend(NULL);
}

TEST_CASE("Flash write waits for blocking mmap to be unmapped", "[flash_dispatcher]")
{
    init_flash_dispatcher();

    mmap_deadlock_test_ctx_t ctx = {
        .mmap_held = xSemaphoreCreateBinary(),
        .allow_unmap = xSemaphoreCreateBinary(),
        .writer_started = xSemaphoreCreateBinary(),
        .holder_done = xSemaphoreCreateBinary(),
        .writer_done = xSemaphoreCreateBinary(),
        .mmap_result = ESP_FAIL,
        .writer_result = ESP_FAIL,
    };
    memset(ctx.write_data, 0xA5, sizeof(ctx.write_data));
    TEST_ASSERT_NOT_NULL(ctx.mmap_held);
    TEST_ASSERT_NOT_NULL(ctx.allow_unmap);
    TEST_ASSERT_NOT_NULL(ctx.writer_started);
    TEST_ASSERT_NOT_NULL(ctx.holder_done);
    TEST_ASSERT_NOT_NULL(ctx.writer_done);

    TaskHandle_t writer_task = NULL;
    TaskHandle_t holder_task = NULL;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCoreWithCaps(flash_writer_task, "flash_writer",
                                                              TEST_PSRAM_TASK_STACK_SIZE, &ctx, 8, &writer_task, 0,
                                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCoreWithCaps(blocking_mmap_holder_task, "mmap_holder",
                                                              TEST_PSRAM_TASK_STACK_SIZE, &ctx, 5, &holder_task, 0,
                                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.writer_started, pdMS_TO_TICKS(2000)));
    wait_task_blocked(writer_task);
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(ctx.writer_done, 0));
    xSemaphoreGive(ctx.allow_unmap);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.writer_done, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.holder_done, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL(ESP_OK, ctx.mmap_result);
    TEST_ASSERT_EQUAL(ESP_OK, ctx.writer_result);

    vTaskDeleteWithCaps(writer_task);
    vTaskDeleteWithCaps(holder_task);
    vSemaphoreDelete(ctx.mmap_held);
    vSemaphoreDelete(ctx.allow_unmap);
    vSemaphoreDelete(ctx.writer_started);
    vSemaphoreDelete(ctx.holder_done);
    vSemaphoreDelete(ctx.writer_done);
}

typedef struct {
    SemaphoreHandle_t write_started;
    SemaphoreHandle_t write_done;
    esp_partition_mmap_handle_t mmap_handle;
    esp_err_t mmap_result;
    esp_err_t write_result;
    uint8_t write_data[16];
} same_task_mmap_write_ctx_t;

static void same_task_mmap_write_task(void *arg)
{
    same_task_mmap_write_ctx_t *ctx = arg;
    const esp_partition_t *partition = get_test_data_partition();
    const void *mmap_ptr = NULL;

    ctx->mmap_result = esp_partition_mmap(partition, 0, TEST_FLASH_SIZE,
                                          ESP_PARTITION_MMAP_DATA | ESP_PARTITION_MMAP_BLOCKS_WRITE,
                                          &mmap_ptr, &ctx->mmap_handle);
    xSemaphoreGive(ctx->write_started);
    if (ctx->mmap_result == ESP_OK) {
        ctx->write_result = esp_partition_write(partition, TEST_FLASH_SIZE,
                                                ctx->write_data, sizeof(ctx->write_data));
    }
    xSemaphoreGive(ctx->write_done);
    vTaskSuspend(NULL);
}

TEST_CASE("Flash write after mmap in same task waits for unmap", "[flash_dispatcher]")
{
    init_flash_dispatcher();

    same_task_mmap_write_ctx_t ctx = {
        .write_started = xSemaphoreCreateBinary(),
        .write_done = xSemaphoreCreateBinary(),
        .mmap_result = ESP_FAIL,
        .write_result = ESP_FAIL,
    };
    memset(ctx.write_data, 0x5A, sizeof(ctx.write_data));
    TEST_ASSERT_NOT_NULL(ctx.write_started);
    TEST_ASSERT_NOT_NULL(ctx.write_done);

    TaskHandle_t task = NULL;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCoreWithCaps(same_task_mmap_write_task, "mmap_write",
                                                              TEST_PSRAM_TASK_STACK_SIZE, &ctx, 5, &task, 0,
                                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.write_started, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL(ESP_OK, ctx.mmap_result);
    wait_task_blocked(task);
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(ctx.write_done, 0));
    esp_partition_munmap(ctx.mmap_handle);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.write_done, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL(ESP_OK, ctx.write_result);

    vTaskDeleteWithCaps(task);
    vSemaphoreDelete(ctx.write_started);
    vSemaphoreDelete(ctx.write_done);
}
#endif
