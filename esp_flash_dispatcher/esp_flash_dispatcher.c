/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_flash_spi_init.h"
#include "esp_private/startup_internal.h"
#include "esp_check.h"
#include "esp_flash_dispatcher.h"

static const char *TAG = "flash_dispatcher";

extern esp_err_t __real_esp_flash_read(esp_flash_t *chip, void *buffer, uint32_t address, uint32_t size);
extern esp_err_t __real_esp_flash_write(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t size);
extern esp_err_t __real_esp_flash_erase_region(esp_flash_t *chip, uint32_t start_address, uint32_t size);
extern esp_err_t __real_esp_flash_erase_chip(esp_flash_t *chip);
extern esp_err_t __real_esp_flash_write_encrypted(esp_flash_t *chip, uint32_t address, const void *buffer, uint32_t length);

// Operation type for flash requests
typedef enum {
    FLASH_OP_READ = 0,
    FLASH_OP_WRITE,
    FLASH_OP_WRITE_ENCRYPTED,
    FLASH_OP_ERASE_REGION,
    FLASH_OP_ERASE_CHIP,
} flash_operation_t;

// Structure to hold flash operation requests
typedef struct {
    esp_flash_t *chip;
    void *arg1;
    void *arg2;
    void *arg3;
    flash_operation_t op;            // Operation type to execute
    TaskHandle_t calling_task_handle; // Task to notify upon completion
    esp_err_t result;                 // Result of the operation
} flash_operation_request_t;

typedef struct {
    QueueHandle_t queue;
    TaskHandle_t task;
    bool dispatcher_initialized;
} flash_dispatcher_context_t;

// Configuration struct is declared in public header

static flash_dispatcher_context_t s_flash_dispatcher_ctx;

static void flash_dispatcher_task(void *arg)
{
    flash_operation_request_t request;

    while (true) {
        if (xQueueReceive(s_flash_dispatcher_ctx.queue, &request, portMAX_DELAY) == pdTRUE) {
            // Execute the actual flash operation based on the operation type and arguments
            switch (request.op) {
            case FLASH_OP_READ:
                request.result = __real_esp_flash_read(request.chip, (void *)request.arg1, (uint32_t)(void *)request.arg2, (size_t)request.arg3);
                break;
            case FLASH_OP_WRITE:
                request.result = __real_esp_flash_write(request.chip, (void *)request.arg1, (uint32_t)(void *)request.arg2, (size_t)request.arg3);
                break;
            case FLASH_OP_WRITE_ENCRYPTED:
                request.result = __real_esp_flash_write_encrypted(request.chip, (uint32_t)(void *)request.arg1, (void *)request.arg2, (size_t)request.arg3);
                break;
            case FLASH_OP_ERASE_REGION:
                request.result = __real_esp_flash_erase_region(request.chip, (size_t)request.arg1, (size_t)request.arg2);
                break;
            case FLASH_OP_ERASE_CHIP:
                request.result = __real_esp_flash_erase_chip(request.chip);
                break;
            default:
                ESP_EARLY_LOGE(TAG, "Unsupported flash operation type: %d", (int)request.op);
                request.result = ESP_FAIL;
                break;
            }
            // Notify the calling task about completion
            if (request.calling_task_handle) {
                xTaskNotify(request.calling_task_handle, request.result, eSetValueWithOverwrite);
            }
        }
    }
}

esp_err_t esp_flash_dispatcher_init(const esp_flash_dispatcher_config_t *cfg)
{
    // If already initialized, return success
    if (s_flash_dispatcher_ctx.queue != NULL || s_flash_dispatcher_ctx.task != NULL) {
        ESP_EARLY_LOGE(TAG, "flash dispatcher already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_flash_dispatcher_ctx.queue = xQueueCreate(cfg->queue_size, sizeof(flash_operation_request_t));
    ESP_RETURN_ON_FALSE(s_flash_dispatcher_ctx.queue, ESP_ERR_NO_MEM, TAG, "create flash operation queue failed");

    BaseType_t rc = xTaskCreatePinnedToCoreWithCaps(flash_dispatcher_task,
                    "flash_dispatcher",
                    cfg->task_stack_size,
                    (void *)xTaskGetCurrentTaskHandle(),
                    cfg->task_priority,
                    &s_flash_dispatcher_ctx.task,
                    cfg->task_core_id,
                    MALLOC_CAP_INTERNAL);

    if (rc != pdPASS) {
        // Cleanup resources if task creation failed
        vQueueDelete(s_flash_dispatcher_ctx.queue);
        s_flash_dispatcher_ctx.queue = NULL;
        ESP_EARLY_LOGE(TAG, "create flash dispatcher task failed");
        return ESP_ERR_INVALID_STATE;
    }

    s_flash_dispatcher_ctx.dispatcher_initialized = true;

    return ESP_OK;
}

/**
 * Send a flash operation request to the dispatcher queue and wait for the result.
 * The meaning and order of arg1/arg2/arg3 must match what the dispatcher task expects
 * for the given operation type. This helper centralizes the common send/wait logic.
 */
static esp_err_t flash_dispatcher_execute(flash_operation_t op,
        esp_flash_t *chip,
        void *arg1,
        void *arg2,
        void *arg3,
        const char *op_name)
{
    ESP_RETURN_ON_FALSE(s_flash_dispatcher_ctx.dispatcher_initialized, ESP_ERR_INVALID_STATE, TAG, "flash dispatcher is not initialized");

    flash_operation_request_t request = {
        .chip = chip,
        .arg1 = arg1,
        .arg2 = arg2,
        .arg3 = arg3,
        .op = op,
        .calling_task_handle = xTaskGetCurrentTaskHandle(),
        .result = ESP_FAIL
    };

    if (xQueueSend(s_flash_dispatcher_ctx.queue, &request, portMAX_DELAY) != pdTRUE) {
        ESP_EARLY_LOGE(TAG, "Failed to send %s request to queue", op_name ? op_name : "flash");
        return ESP_ERR_TIMEOUT;
    }

    uint32_t notification_value = ESP_FAIL;
    xTaskNotifyWait(0, 0xFFFFFFFF, &notification_value, portMAX_DELAY);
    return (esp_err_t)notification_value;
}

esp_err_t __wrap_esp_flash_read(esp_flash_t *chip, void *buffer, uint32_t address, uint32_t size)
{
    return flash_dispatcher_execute(FLASH_OP_READ,
                                    chip,
                                    (void *)buffer,
                                    (void *)address,
                                    (void *)size,
                                    "flash read");
}

esp_err_t __wrap_esp_flash_write(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t size)
{
    return flash_dispatcher_execute(FLASH_OP_WRITE,
                                    chip,
                                    (void *)buffer,
                                    (void *)address,
                                    (void *)size,
                                    "flash write");
}

esp_err_t __wrap_esp_flash_write_encrypted(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t size)
{
    return flash_dispatcher_execute(FLASH_OP_WRITE_ENCRYPTED,
                                    chip,
                                    (void *)address,
                                    (void *)buffer,
                                    (void *)size,
                                    "flash write_encrypted");
}

esp_err_t __wrap_esp_flash_erase_region(esp_flash_t *chip, uint32_t start_address, uint32_t size)
{
    return flash_dispatcher_execute(FLASH_OP_ERASE_REGION,
                                    chip,
                                    (void *)start_address,
                                    (void *)size,
                                    NULL,
                                    "flash erase_region");
}

esp_err_t __wrap_esp_flash_erase_chip(esp_flash_t *chip)
{
    return flash_dispatcher_execute(FLASH_OP_ERASE_CHIP,
                                    chip,
                                    NULL,
                                    NULL,
                                    NULL,
                                    "flash erase_chip");
}
