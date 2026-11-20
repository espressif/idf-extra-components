/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_flash_spi_init.h"
#include "esp_private/startup_internal.h"
#include "esp_check.h"
#include "esp_flash_dispatcher.h"
#include "spi_flash_mmap.h"
#include "esp_private/spi_flash_os.h"
#include "esp_memory_utils.h"

static const char *TAG = "flash_dispatcher";

// In no-OS contexts (e.g. coredump, panic handler) the scheduler is gone, so
// the dispatcher task can never run. Detect it and call the real flash ops directly.
static inline bool flash_dispatcher_is_no_os(void)
{
    return spi_flash_guard_get() == &g_flash_guard_no_os_ops;
}

extern esp_err_t __real_esp_flash_read(esp_flash_t *chip, void *buffer, uint32_t address, uint32_t size);
extern esp_err_t __real_esp_flash_write(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t size);
extern esp_err_t __real_esp_flash_erase_region(esp_flash_t *chip, uint32_t start_address, uint32_t size);
extern esp_err_t __real_esp_flash_erase_chip(esp_flash_t *chip);
extern esp_err_t __real_esp_flash_write_encrypted(esp_flash_t *chip, uint32_t address, const void *buffer, uint32_t length);
extern esp_err_t __real_spi_flash_mmap(size_t src_addr, size_t size, spi_flash_mmap_memory_t memory,
                                       const void **out_ptr, spi_flash_mmap_handle_t *out_handle);
extern esp_err_t __real_spi_flash_mmap_pages(const int *pages, size_t page_count, spi_flash_mmap_memory_t memory,
                                             const void **out_ptr, spi_flash_mmap_handle_t *out_handle);
extern void __real_spi_flash_munmap(spi_flash_mmap_handle_t handle);

#define FLASH_DISPATCHER_MMAP_BLOCKS_WRITE (1U << 1)
#define FLASH_DISPATCHER_MMAP_IDLE_BIT      (1U << 0)

// Operation type for flash requests
typedef enum {
    FLASH_OP_READ = 0,
    FLASH_OP_WRITE,
    FLASH_OP_WRITE_ENCRYPTED,
    FLASH_OP_ERASE_REGION,
    FLASH_OP_ERASE_CHIP,
    FLASH_OP_MMAP,
    FLASH_OP_MMAP_PAGES,
    FLASH_OP_MUNMAP,
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
        struct {
            size_t src_addr;
            size_t size;
            spi_flash_mmap_memory_t memory;
            const void **out_ptr;
            spi_flash_mmap_handle_t *out_handle;
        } mmap;                         // for FLASH_OP_MMAP
        struct {
            const int *pages;
            size_t page_count;
            spi_flash_mmap_memory_t memory;
            const void **out_ptr;
            spi_flash_mmap_handle_t *out_handle;
        } mmap_pages;                   // for FLASH_OP_MMAP_PAGES
        struct {
            spi_flash_mmap_handle_t handle;
        } munmap;                       // for FLASH_OP_MUNMAP
    } args;
} flash_operation_request_t;

typedef struct flash_mmap_record {
    spi_flash_mmap_handle_t handle;
    struct flash_mmap_record *next;
} flash_mmap_record_t;

typedef struct {
    flash_operation_request_t request;  // request slot
    esp_err_t result;                   // result slot
    TaskHandle_t task;
    SemaphoreHandle_t dispatch_mutex;   // serialize concurrent callers
    SemaphoreHandle_t mmap_mutex;       // protect blocking mmap tracking
    EventGroupHandle_t mmap_event;       // signal no blocking mmap
    SemaphoreHandle_t request_sem;      // binary semaphore
    SemaphoreHandle_t result_sem;       // binary semaphore
    flash_mmap_record_t *blocking_mmaps;
    uint32_t pending_blocking_mmaps;
    bool dispatcher_initialized;
} flash_dispatcher_context_t;

// Configuration struct is declared in public header

static flash_dispatcher_context_t s_flash_dispatcher_ctx;

static inline bool flash_dispatcher_can_use_os(void)
{
    return !flash_dispatcher_is_no_os() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
}

static inline bool flash_dispatcher_should_bypass(void)
{
    return !flash_dispatcher_can_use_os() || esp_ptr_in_dram(esp_cpu_get_sp());
}

static bool flash_operation_writes(flash_operation_t op)
{
    return op == FLASH_OP_WRITE || op == FLASH_OP_WRITE_ENCRYPTED ||
           op == FLASH_OP_ERASE_REGION || op == FLASH_OP_ERASE_CHIP;
}

static bool flash_mmap_blocks_write(spi_flash_mmap_memory_t memory)
{
#if CONFIG_IDF_TARGET_ESP32
    (void)memory;
    return false;
#else
    return ((uint32_t)memory & FLASH_DISPATCHER_MMAP_BLOCKS_WRITE) != 0;
#endif
}

static bool flash_operation_adds_blocking_mmap(const flash_operation_request_t *request)
{
    if (request->op == FLASH_OP_MMAP) {
        return flash_mmap_blocks_write(request->args.mmap.memory);
    }
    if (request->op == FLASH_OP_MMAP_PAGES) {
        return flash_mmap_blocks_write(request->args.mmap_pages.memory);
    }
    return false;
}

static spi_flash_mmap_handle_t *flash_operation_mmap_handle(const flash_operation_request_t *request)
{
    return request->op == FLASH_OP_MMAP ?
           request->args.mmap.out_handle : request->args.mmap_pages.out_handle;
}

static void flash_dispatcher_update_mmap_idle_locked(void)
{
    if (s_flash_dispatcher_ctx.blocking_mmaps == NULL && s_flash_dispatcher_ctx.pending_blocking_mmaps == 0) {
        xEventGroupSetBits(s_flash_dispatcher_ctx.mmap_event, FLASH_DISPATCHER_MMAP_IDLE_BIT);
    } else {
        xEventGroupClearBits(s_flash_dispatcher_ctx.mmap_event, FLASH_DISPATCHER_MMAP_IDLE_BIT);
    }
}

static void flash_dispatcher_begin_blocking_mmap(void)
{
    xSemaphoreTake(s_flash_dispatcher_ctx.mmap_mutex, portMAX_DELAY);
    s_flash_dispatcher_ctx.pending_blocking_mmaps++;
    xEventGroupClearBits(s_flash_dispatcher_ctx.mmap_event, FLASH_DISPATCHER_MMAP_IDLE_BIT);
    xSemaphoreGive(s_flash_dispatcher_ctx.mmap_mutex);
}

static void flash_dispatcher_finish_blocking_mmap(flash_mmap_record_t *record)
{
    xSemaphoreTake(s_flash_dispatcher_ctx.mmap_mutex, portMAX_DELAY);
    s_flash_dispatcher_ctx.pending_blocking_mmaps--;
    if (record != NULL) {
        record->next = s_flash_dispatcher_ctx.blocking_mmaps;
        s_flash_dispatcher_ctx.blocking_mmaps = record;
    }
    flash_dispatcher_update_mmap_idle_locked();
    xSemaphoreGive(s_flash_dispatcher_ctx.mmap_mutex);
}

static void flash_dispatcher_remove_mmap(spi_flash_mmap_handle_t handle)
{
    flash_mmap_record_t *removed = NULL;

    xSemaphoreTake(s_flash_dispatcher_ctx.mmap_mutex, portMAX_DELAY);
    flash_mmap_record_t **record = &s_flash_dispatcher_ctx.blocking_mmaps;
    while (*record != NULL) {
        if ((*record)->handle == handle) {
            removed = *record;
            *record = removed->next;
            break;
        }
        record = &(*record)->next;
    }
    flash_dispatcher_update_mmap_idle_locked();
    xSemaphoreGive(s_flash_dispatcher_ctx.mmap_mutex);
    heap_caps_free(removed);
}

static bool flash_dispatcher_has_blocking_mmap(void)
{
    xSemaphoreTake(s_flash_dispatcher_ctx.mmap_mutex, portMAX_DELAY);
    bool has_blocking = s_flash_dispatcher_ctx.blocking_mmaps != NULL ||
                        s_flash_dispatcher_ctx.pending_blocking_mmaps > 0;
    xSemaphoreGive(s_flash_dispatcher_ctx.mmap_mutex);
    return has_blocking;
}

static void flash_dispatcher_task(void *arg)
{
    while (true) {
        xSemaphoreTake(s_flash_dispatcher_ctx.request_sem, portMAX_DELAY);

        const flash_operation_request_t *req = &s_flash_dispatcher_ctx.request;
        esp_err_t result;

        switch (req->op) {
        case FLASH_OP_READ:
            result = __real_esp_flash_read(req->chip,
                                           req->args.read.buffer,
                                           req->args.read.address,
                                           req->args.read.size);
            break;
        case FLASH_OP_WRITE:
            result = __real_esp_flash_write(req->chip,
                                            req->args.write.buffer,
                                            req->args.write.address,
                                            req->args.write.size);
            break;
        case FLASH_OP_WRITE_ENCRYPTED:
            result = __real_esp_flash_write_encrypted(req->chip,
                                                      req->args.write_encrypted.address,
                                                      req->args.write_encrypted.buffer,
                                                      req->args.write_encrypted.size);
            break;
        case FLASH_OP_ERASE_REGION:
            result = __real_esp_flash_erase_region(req->chip,
                                                   req->args.erase_region.start_address,
                                                   req->args.erase_region.size);
            break;
        case FLASH_OP_ERASE_CHIP:
            result = __real_esp_flash_erase_chip(req->chip);
            break;
        case FLASH_OP_MMAP:
            result = __real_spi_flash_mmap(req->args.mmap.src_addr,
                                           req->args.mmap.size,
                                           req->args.mmap.memory,
                                           req->args.mmap.out_ptr,
                                           req->args.mmap.out_handle);
            break;
        case FLASH_OP_MMAP_PAGES:
            result = __real_spi_flash_mmap_pages(req->args.mmap_pages.pages,
                                                 req->args.mmap_pages.page_count,
                                                 req->args.mmap_pages.memory,
                                                 req->args.mmap_pages.out_ptr,
                                                 req->args.mmap_pages.out_handle);
            break;
        case FLASH_OP_MUNMAP:
            __real_spi_flash_munmap(req->args.munmap.handle);
            result = ESP_OK;
            break;
        default:
            ESP_EARLY_LOGE(TAG, "Unsupported flash operation type: %d", (int)req->op);
            result = ESP_FAIL;
            break;
        }

        // Publish result and wake up the caller waiting on result_sem.
        s_flash_dispatcher_ctx.result = result;
        xSemaphoreGive(s_flash_dispatcher_ctx.result_sem);
    }
}

esp_err_t esp_flash_dispatcher_init(const esp_flash_dispatcher_config_t *cfg)
{
    if (s_flash_dispatcher_ctx.dispatcher_initialized) {
        ESP_EARLY_LOGE(TAG, "flash dispatcher already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_ERR_NO_MEM;
    s_flash_dispatcher_ctx.dispatch_mutex = xSemaphoreCreateMutexWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(s_flash_dispatcher_ctx.dispatch_mutex, ESP_ERR_NO_MEM, err, TAG, "create dispatch mutex failed");

    s_flash_dispatcher_ctx.mmap_mutex = xSemaphoreCreateMutexWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(s_flash_dispatcher_ctx.mmap_mutex, ESP_ERR_NO_MEM, err, TAG, "create mmap mutex failed");

    s_flash_dispatcher_ctx.request_sem = xSemaphoreCreateBinaryWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(s_flash_dispatcher_ctx.request_sem, ESP_ERR_NO_MEM, err, TAG, "create request semaphore failed");

    s_flash_dispatcher_ctx.result_sem = xSemaphoreCreateBinaryWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(s_flash_dispatcher_ctx.result_sem, ESP_ERR_NO_MEM, err, TAG, "create result semaphore failed");

    s_flash_dispatcher_ctx.mmap_event = xEventGroupCreateWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(s_flash_dispatcher_ctx.mmap_event, ESP_ERR_NO_MEM, err, TAG, "create mmap event failed");
    xEventGroupSetBits(s_flash_dispatcher_ctx.mmap_event, FLASH_DISPATCHER_MMAP_IDLE_BIT);

    BaseType_t rc = xTaskCreatePinnedToCoreWithCaps(flash_dispatcher_task,
                                                    "flash_dispatcher",
                                                    cfg->task_stack_size,
                                                    NULL,
                                                    cfg->task_priority,
                                                    &s_flash_dispatcher_ctx.task,
                                                    cfg->task_core_id,
                                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(rc == pdPASS, ESP_ERR_INVALID_STATE, err, TAG, "create flash dispatcher task failed");

    s_flash_dispatcher_ctx.dispatcher_initialized = true;
    return ESP_OK;

err:
    if (s_flash_dispatcher_ctx.dispatch_mutex != NULL) {
        vSemaphoreDeleteWithCaps(s_flash_dispatcher_ctx.dispatch_mutex);
        s_flash_dispatcher_ctx.dispatch_mutex = NULL;
    }
    if (s_flash_dispatcher_ctx.mmap_mutex != NULL) {
        vSemaphoreDeleteWithCaps(s_flash_dispatcher_ctx.mmap_mutex);
        s_flash_dispatcher_ctx.mmap_mutex = NULL;
    }
    if (s_flash_dispatcher_ctx.request_sem != NULL) {
        vSemaphoreDeleteWithCaps(s_flash_dispatcher_ctx.request_sem);
        s_flash_dispatcher_ctx.request_sem = NULL;
    }
    if (s_flash_dispatcher_ctx.result_sem != NULL) {
        vSemaphoreDeleteWithCaps(s_flash_dispatcher_ctx.result_sem);
        s_flash_dispatcher_ctx.result_sem = NULL;
    }
    if (s_flash_dispatcher_ctx.mmap_event != NULL) {
        vEventGroupDeleteWithCaps(s_flash_dispatcher_ctx.mmap_event);
        s_flash_dispatcher_ctx.mmap_event = NULL;
    }
    return ret;
}

/**
 * Send a flash operation request to the dispatcher queue and wait for the result.
 * The meaning and order of arg1/arg2/arg3 must match what the dispatcher task expects
 * for the given operation type. This helper centralizes the common send/wait logic.
 */
static esp_err_t flash_dispatcher_execute(const flash_operation_request_t *request, const char *op_name)
{
    ESP_RETURN_ON_FALSE(s_flash_dispatcher_ctx.dispatcher_initialized, ESP_ERR_INVALID_STATE, TAG, "flash dispatcher is not initialized");

    flash_mmap_record_t *new_mmap = NULL;
    if (flash_operation_adds_blocking_mmap(request)) {
        new_mmap = heap_caps_malloc(sizeof(flash_mmap_record_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(new_mmap, ESP_ERR_NO_MEM, TAG, "allocate mmap record failed");
    }

    while (true) {
        xSemaphoreTake(s_flash_dispatcher_ctx.dispatch_mutex, portMAX_DELAY);
        if (!flash_operation_writes(request->op) || !flash_dispatcher_has_blocking_mmap()) {
            break;
        }
        xSemaphoreGive(s_flash_dispatcher_ctx.dispatch_mutex);
        xEventGroupWaitBits(s_flash_dispatcher_ctx.mmap_event,
                            FLASH_DISPATCHER_MMAP_IDLE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }

    if (new_mmap != NULL) {
        flash_dispatcher_begin_blocking_mmap();
    }

    // Publish the request into the shared slot and wake the worker.
    s_flash_dispatcher_ctx.request = *request;
    xSemaphoreGive(s_flash_dispatcher_ctx.request_sem);

    if (xSemaphoreTake(s_flash_dispatcher_ctx.result_sem, portMAX_DELAY) != pdTRUE) {
        if (new_mmap != NULL) {
            flash_dispatcher_finish_blocking_mmap(NULL);
        }
        xSemaphoreGive(s_flash_dispatcher_ctx.dispatch_mutex);
        heap_caps_free(new_mmap);
        ESP_EARLY_LOGE(TAG, "Failed to receive %s result from queue", op_name ? op_name : "flash");
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t operation_result = s_flash_dispatcher_ctx.result;

    if (new_mmap != NULL) {
        spi_flash_mmap_handle_t *out_handle = flash_operation_mmap_handle(request);
        if (operation_result == ESP_OK && out_handle != NULL) {
            new_mmap->handle = *out_handle;
            flash_dispatcher_finish_blocking_mmap(new_mmap);
            new_mmap = NULL;
        } else {
            flash_dispatcher_finish_blocking_mmap(NULL);
        }
    } else if (operation_result == ESP_OK && request->op == FLASH_OP_MUNMAP) {
        flash_dispatcher_remove_mmap(request->args.munmap.handle);
    }

    xSemaphoreGive(s_flash_dispatcher_ctx.dispatch_mutex);
    heap_caps_free(new_mmap);
    return operation_result;
}

esp_err_t __wrap_esp_flash_read(esp_flash_t *chip, void *buffer, uint32_t address, uint32_t size)
{
    if (flash_dispatcher_should_bypass()) {
        return __real_esp_flash_read(chip, buffer, address, size);
    }
    flash_operation_request_t request = {
        .chip = chip,
        .op = FLASH_OP_READ,
        .args.read = {
            .buffer = buffer,
            .address = address,
            .size = size,
        },
    };
    return flash_dispatcher_execute(&request, "flash read");
}

esp_err_t __wrap_esp_flash_write(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t size)
{
    if (flash_dispatcher_should_bypass()) {
        return __real_esp_flash_write(chip, buffer, address, size);
    }
    flash_operation_request_t request = {
        .chip = chip,
        .op = FLASH_OP_WRITE,
        .args.write = {
            .buffer = buffer,
            .address = address,
            .size = size,
        },
    };
    return flash_dispatcher_execute(&request, "flash write");
}

esp_err_t __wrap_esp_flash_write_encrypted(esp_flash_t *chip, uint32_t address, const void *buffer, uint32_t size)
{
    if (flash_dispatcher_should_bypass()) {
        return __real_esp_flash_write_encrypted(chip, address, buffer, size);
    }
    flash_operation_request_t request = {
        .chip = chip,
        .op = FLASH_OP_WRITE_ENCRYPTED,
        .args.write_encrypted = {
            .address = address,
            .buffer = buffer,
            .size = size,
        },
    };
    return flash_dispatcher_execute(&request, "flash write_encrypted");
}

esp_err_t __wrap_esp_flash_erase_region(esp_flash_t *chip, uint32_t start_address, uint32_t size)
{
    if (flash_dispatcher_should_bypass()) {
        return __real_esp_flash_erase_region(chip, start_address, size);
    }
    flash_operation_request_t request = {
        .chip = chip,
        .op = FLASH_OP_ERASE_REGION,
        .args.erase_region = {
            .start_address = start_address,
            .size = size,
        },
    };
    return flash_dispatcher_execute(&request, "flash erase_region");
}

esp_err_t __wrap_esp_flash_erase_chip(esp_flash_t *chip)
{
    if (flash_dispatcher_should_bypass()) {
        return __real_esp_flash_erase_chip(chip);
    }
    flash_operation_request_t request = {
        .chip = chip,
        .op = FLASH_OP_ERASE_CHIP,
    };
    return flash_dispatcher_execute(&request, "flash erase_chip");
}

esp_err_t __wrap_spi_flash_mmap(size_t src_addr, size_t size, spi_flash_mmap_memory_t memory,
                                const void **out_ptr, spi_flash_mmap_handle_t *out_handle)
{
    if (flash_dispatcher_should_bypass()) {
        if (!flash_dispatcher_can_use_os() || !s_flash_dispatcher_ctx.dispatcher_initialized || !flash_mmap_blocks_write(memory)) {
            return __real_spi_flash_mmap(src_addr, size, memory, out_ptr, out_handle);
        }
        flash_mmap_record_t *new_mmap = heap_caps_malloc(sizeof(flash_mmap_record_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(new_mmap, ESP_ERR_NO_MEM, TAG, "allocate mmap record failed");
        flash_dispatcher_begin_blocking_mmap();
        esp_err_t ret = __real_spi_flash_mmap(src_addr, size, memory, out_ptr, out_handle);
        if (ret == ESP_OK && out_handle != NULL) {
            new_mmap->handle = *out_handle;
            flash_dispatcher_finish_blocking_mmap(new_mmap);
        } else {
            flash_dispatcher_finish_blocking_mmap(NULL);
            heap_caps_free(new_mmap);
        }
        return ret;
    }
    flash_operation_request_t request = {
        .chip = NULL,
        .op = FLASH_OP_MMAP,
        .args.mmap = {
            .src_addr = src_addr,
            .size = size,
            .memory = memory,
            .out_ptr = out_ptr,
            .out_handle = out_handle,
        },
    };
    return flash_dispatcher_execute(&request, "spi_flash_mmap");
}

esp_err_t __wrap_spi_flash_mmap_pages(const int *pages, size_t page_count, spi_flash_mmap_memory_t memory,
                                      const void **out_ptr, spi_flash_mmap_handle_t *out_handle)
{
    if (flash_dispatcher_should_bypass()) {
        if (!flash_dispatcher_can_use_os() || !s_flash_dispatcher_ctx.dispatcher_initialized || !flash_mmap_blocks_write(memory)) {
            return __real_spi_flash_mmap_pages(pages, page_count, memory, out_ptr, out_handle);
        }
        flash_mmap_record_t *new_mmap = heap_caps_malloc(sizeof(flash_mmap_record_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(new_mmap, ESP_ERR_NO_MEM, TAG, "allocate mmap record failed");
        flash_dispatcher_begin_blocking_mmap();
        esp_err_t ret = __real_spi_flash_mmap_pages(pages, page_count, memory, out_ptr, out_handle);
        if (ret == ESP_OK && out_handle != NULL) {
            new_mmap->handle = *out_handle;
            flash_dispatcher_finish_blocking_mmap(new_mmap);
        } else {
            flash_dispatcher_finish_blocking_mmap(NULL);
            heap_caps_free(new_mmap);
        }
        return ret;
    }
    flash_operation_request_t request = {
        .chip = NULL,
        .op = FLASH_OP_MMAP_PAGES,
        .args.mmap_pages = {
            .pages = pages,
            .page_count = page_count,
            .memory = memory,
            .out_ptr = out_ptr,
            .out_handle = out_handle,
        },
    };
    return flash_dispatcher_execute(&request, "spi_flash_mmap_pages");
}

void __wrap_spi_flash_munmap(spi_flash_mmap_handle_t handle)
{
    if (flash_dispatcher_should_bypass()) {
        __real_spi_flash_munmap(handle);
        if (flash_dispatcher_can_use_os() && s_flash_dispatcher_ctx.dispatcher_initialized) {
            flash_dispatcher_remove_mmap(handle);
        }
        return;
    }
    flash_operation_request_t request = {
        .chip = NULL,
        .op = FLASH_OP_MUNMAP,
        .args.munmap = {
            .handle = handle,
        },
    };
    (void)flash_dispatcher_execute(&request, "spi_flash_munmap");
}
