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
    flash_operation_t op;            // Operation type to execute
    union {
        struct {
            void *buffer;
            uint32_t address;
            size_t size;
        } read;                 // for FLASH_OP_READ
        struct {
            const void *buffer;
            uint32_t address;
            size_t size;
        } write;          // for FLASH_OP_WRITE
        struct {
            uint32_t address;
            const void *buffer;
            size_t size;
        } write_encrypted;// for FLASH_OP_WRITE_ENCRYPTED
        struct {
            uint32_t start_address;
            size_t size;
        } erase_region;                 // for FLASH_OP_ERASE_REGION
    } args;
} flash_operation_request_t;

typedef struct {
    QueueHandle_t queue;
    TaskHandle_t task;
    bool dispatcher_initialized;
    QueueHandle_t result_queue;
} flash_dispatcher_context_t;

// Configuration struct is declared in public header

static flash_dispatcher_context_t s_flash_dispatcher_ctx;

static void flash_dispatcher_task(void *arg)
{
    flash_operation_request_t request;
    esp_err_t result;

    while (true) {
        if (xQueueReceive(s_flash_dispatcher_ctx.queue, &request, portMAX_DELAY) == pdTRUE) {
            // Execute the actual flash operation based on the operation type and arguments
            switch (request.op) {
            case FLASH_OP_READ:
                result = __real_esp_flash_read(request.chip,
                                               request.args.read.buffer,
                                               request.args.read.address,
                                               request.args.read.size);
                break;
            case FLASH_OP_WRITE:
                result = __real_esp_flash_write(request.chip,
                                                request.args.write.buffer,
                                                request.args.write.address,
                                                request.args.write.size);
                break;
            case FLASH_OP_WRITE_ENCRYPTED:
                result = __real_esp_flash_write_encrypted(request.chip,
                         request.args.write_encrypted.address,
                         request.args.write_encrypted.buffer,
                         request.args.write_encrypted.size);
                break;
            case FLASH_OP_ERASE_REGION:
                result = __real_esp_flash_erase_region(request.chip,
                                                       request.args.erase_region.start_address,
                                                       request.args.erase_region.size);
                break;
            case FLASH_OP_ERASE_CHIP:
                result = __real_esp_flash_erase_chip(request.chip);
                break;
            default:
                ESP_EARLY_LOGE(TAG, "Unsupported flash operation type: %d", (int)request.op);
                result = ESP_FAIL;
                break;
            }
            // Publish result and signal completion to the waiting caller
            if (xQueueSend(s_flash_dispatcher_ctx.result_queue, &result, portMAX_DELAY) != pdTRUE) {
                ESP_EARLY_LOGE(TAG, "Failed to send result to queue");
            }
        }
    }
}

esp_err_t esp_flash_dispatcher_init(const esp_flash_dispatcher_config_t *cfg)
{
    if (s_flash_dispatcher_ctx.queue != NULL || s_flash_dispatcher_ctx.task != NULL) {
        ESP_EARLY_LOGE(TAG, "flash dispatcher already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_flash_dispatcher_ctx.queue = xQueueCreateWithCaps(cfg->queue_size, sizeof(flash_operation_request_t), MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(s_flash_dispatcher_ctx.queue, ESP_ERR_NO_MEM, TAG, "create flash operation queue failed");

    s_flash_dispatcher_ctx.result_queue = xQueueCreateWithCaps(cfg->queue_size, sizeof(esp_err_t), MALLOC_CAP_INTERNAL);
    if (s_flash_dispatcher_ctx.result_queue == NULL) {
        vQueueDeleteWithCaps(s_flash_dispatcher_ctx.queue);
        ESP_EARLY_LOGE(TAG, "Failed to create completion semaphore");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t rc = xTaskCreatePinnedToCoreWithCaps(flash_dispatcher_task,
                    "flash_dispatcher",
                    cfg->task_stack_size,
                    NULL,
                    cfg->task_priority,
                    &s_flash_dispatcher_ctx.task,
                    cfg->task_core_id,
                    MALLOC_CAP_INTERNAL);

    if (rc != pdPASS) {
        // Cleanup resources if task creation failed
        vQueueDeleteWithCaps(s_flash_dispatcher_ctx.queue);
        s_flash_dispatcher_ctx.queue = NULL;
        vQueueDeleteWithCaps(s_flash_dispatcher_ctx.result_queue);
        s_flash_dispatcher_ctx.result_queue = NULL;
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

    esp_err_t operation_result = ESP_FAIL;
    flash_operation_request_t request = { 0 };
    request.chip = chip;
    request.op = op;
    switch (op) {
    case FLASH_OP_READ:
        request.args.read.buffer = arg1;
        request.args.read.address = (uint32_t)(uintptr_t)arg2;
        request.args.read.size = (size_t)(uintptr_t)arg3;
        break;
    case FLASH_OP_WRITE:
        request.args.write.buffer = arg1;
        request.args.write.address = (uint32_t)(uintptr_t)arg2;
        request.args.write.size = (size_t)(uintptr_t)arg3;
        break;
    case FLASH_OP_WRITE_ENCRYPTED:
        request.args.write_encrypted.address = (uint32_t)(uintptr_t)arg1;
        request.args.write_encrypted.buffer = arg2;
        request.args.write_encrypted.size = (size_t)(uintptr_t)arg3;
        break;
    case FLASH_OP_ERASE_REGION:
        request.args.erase_region.start_address = (uint32_t)(uintptr_t)arg1;
        request.args.erase_region.size = (size_t)(uintptr_t)arg2;
        break;
    case FLASH_OP_ERASE_CHIP:
    default:
        break;
    }

    if (xQueueSend(s_flash_dispatcher_ctx.queue, &request, portMAX_DELAY) != pdTRUE) {
        ESP_EARLY_LOGE(TAG, "Failed to send %s request to queue", op_name ? op_name : "flash");
        return ESP_ERR_TIMEOUT;
    }

    if (xQueueReceive(s_flash_dispatcher_ctx.result_queue, &operation_result, portMAX_DELAY) != pdTRUE) {
        ESP_EARLY_LOGE(TAG, "Failed to receive %s result from queue", op_name ? op_name : "flash");
        return ESP_ERR_TIMEOUT;
    }
    return operation_result;
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

esp_err_t __wrap_esp_flash_write_encrypted(esp_flash_t *chip, uint32_t address, const void *buffer, uint32_t size)
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
