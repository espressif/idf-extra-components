/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "unity.h"
#include "driver/gptimer.h"
#include "esp_intr_alloc.h"
#include "esp_rom_sys.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_app_trace.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

const static char *TAG = "esp_sysview_test";

typedef struct {
    gptimer_handle_t gptimer;
    uint32_t period;
    int flags;
    uint32_t id;
} esp_sysviewtrace_timer_arg_t;

typedef struct {
    SemaphoreHandle_t done;
    SemaphoreHandle_t *sync;
    esp_sysviewtrace_timer_arg_t *timer;
    uint32_t work_count;
    uint32_t sleep_tmo;
    uint32_t id;
} esp_sysviewtrace_task_arg_t;

static bool esp_sysview_test_timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    esp_sysviewtrace_timer_arg_t *tim_arg = (esp_sysviewtrace_timer_arg_t *)user_ctx;
    (void) tim_arg;
    return false;
}

static void esp_sysviewtrace_test_task(void *p)
{
    esp_sysviewtrace_task_arg_t *arg = (esp_sysviewtrace_task_arg_t *) p;
    volatile uint32_t tmp = 0;
    printf("%p: run sysview task\n", xTaskGetCurrentTaskHandle());

    if (arg->timer) {
        gptimer_alarm_config_t alarm_config = {
            .reload_count = 0,
            .alarm_count = arg->timer->period,
            .flags.auto_reload_on_alarm = true,
        };
        gptimer_event_callbacks_t cbs = {
            .on_alarm = esp_sysview_test_timer_isr,
        };
        TEST_ESP_OK(gptimer_register_event_callbacks(arg->timer->gptimer, &cbs, arg->timer));
        TEST_ESP_OK(gptimer_enable(arg->timer->gptimer));
        TEST_ESP_OK(gptimer_set_alarm_action(arg->timer->gptimer, &alarm_config));
        TEST_ESP_OK(gptimer_start(arg->timer->gptimer));
    }

    int i = 0;
    while (1) {
        static uint32_t count;
        printf("%" PRIu32, arg->id);
        if ((++count % 80) == 0) {
            printf("\n");
        }
        if (arg->sync) {
            xSemaphoreTake(*arg->sync, portMAX_DELAY);
        }
        for (uint32_t k = 0; k < arg->work_count; k++) {
            tmp++;
        }
        vTaskDelay(arg->sleep_tmo / portTICK_PERIOD_MS);
        i++;
        if (arg->sync) {
            xSemaphoreGive(*arg->sync);
        }
    }
    ESP_EARLY_LOGI(TAG, "%p: finished", xTaskGetCurrentTaskHandle());

    xSemaphoreGive(arg->done);
    vTaskDelay(1);
    vTaskDelete(NULL);
}

TEST_CASE("SysView trace test 1", "[trace][ignore]")
{
    TaskHandle_t thnd;

    esp_sysviewtrace_timer_arg_t tim_arg1 = {
        .flags = ESP_INTR_FLAG_SHARED,
        .id = 0,
        .period = 500,
    };
    esp_sysviewtrace_task_arg_t arg1 = {
        .done = xSemaphoreCreateBinary(),
        .sync = NULL,
        .work_count = 10000,
        .sleep_tmo = 1,
        .timer = &tim_arg1,
        .id = 0,
    };
    esp_sysviewtrace_timer_arg_t tim_arg2 = {
        .flags = 0,
        .id = 1,
        .period = 100,
    };
    esp_sysviewtrace_task_arg_t arg2 = {
        .done = xSemaphoreCreateBinary(),
        .sync = NULL,
        .work_count = 10000,
        .sleep_tmo = 1,
        .timer = &tim_arg2,
        .id = 1,
    };

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    timer_config.flags.intr_shared = (tim_arg1.flags & ESP_INTR_FLAG_SHARED) == ESP_INTR_FLAG_SHARED;
    TEST_ESP_OK(gptimer_new_timer(&timer_config, &tim_arg1.gptimer));
    timer_config.flags.intr_shared = (tim_arg2.flags & ESP_INTR_FLAG_SHARED) == ESP_INTR_FLAG_SHARED;
    TEST_ESP_OK(gptimer_new_timer(&timer_config, &tim_arg2.gptimer));

    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace0", 2048, &arg1, 3, &thnd, 0);
    ESP_EARLY_LOGI(TAG, "Created task %p", thnd);
#if CONFIG_FREERTOS_UNICORE == 0
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace1", 2048, &arg2, 5, &thnd, 1);
#else
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace1", 2048, &arg2, 5, &thnd, 0);
#endif
    ESP_EARLY_LOGI(TAG, "Created task %p", thnd);

    xSemaphoreTake(arg1.done, portMAX_DELAY);
    vSemaphoreDelete(arg1.done);
    xSemaphoreTake(arg2.done, portMAX_DELAY);
    vSemaphoreDelete(arg2.done);
    TEST_ESP_OK(gptimer_stop(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_disable(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_del_timer(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_stop(tim_arg2.gptimer));
    TEST_ESP_OK(gptimer_disable(tim_arg2.gptimer));
    TEST_ESP_OK(gptimer_del_timer(tim_arg2.gptimer));
}

TEST_CASE("SysView trace test 2", "[trace][ignore]")
{
    TaskHandle_t thnd;

    esp_sysviewtrace_timer_arg_t tim_arg1 = {
        .flags = ESP_INTR_FLAG_SHARED,
        .id = 0,
        .period = 500,
    };
    esp_sysviewtrace_task_arg_t arg1 = {
        .done = xSemaphoreCreateBinary(),
        .sync = NULL,
        .work_count = 10000,
        .sleep_tmo = 1,
        .timer = &tim_arg1,
        .id = 0,
    };
    esp_sysviewtrace_timer_arg_t tim_arg2 = {
        .flags = 0,
        .id = 1,
        .period = 100,
    };
    esp_sysviewtrace_task_arg_t arg2 = {
        .done = xSemaphoreCreateBinary(),
        .sync = NULL,
        .work_count = 10000,
        .sleep_tmo = 1,
        .timer = &tim_arg2,
        .id = 1,
    };

    SemaphoreHandle_t test_sync = xSemaphoreCreateBinary();
    xSemaphoreGive(test_sync);
    esp_sysviewtrace_task_arg_t arg3 = {
        .done = xSemaphoreCreateBinary(),
        .sync = &test_sync,
        .work_count = 1000,
        .sleep_tmo = 1,
        .timer = NULL,
        .id = 2,
    };
    esp_sysviewtrace_task_arg_t arg4 = {
        .done = xSemaphoreCreateBinary(),
        .sync = &test_sync,
        .work_count = 10000,
        .sleep_tmo = 1,
        .timer = NULL,
        .id = 3,
    };

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    timer_config.flags.intr_shared = (tim_arg1.flags & ESP_INTR_FLAG_SHARED) == ESP_INTR_FLAG_SHARED;
    TEST_ESP_OK(gptimer_new_timer(&timer_config, &tim_arg1.gptimer));
    timer_config.flags.intr_shared = (tim_arg2.flags & ESP_INTR_FLAG_SHARED) == ESP_INTR_FLAG_SHARED;
    TEST_ESP_OK(gptimer_new_timer(&timer_config, &tim_arg2.gptimer));

    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace0", 2048, &arg1, 3, &thnd, 0);
    printf("Created task %p\n", thnd);
#if CONFIG_FREERTOS_UNICORE == 0
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace1", 2048, &arg2, 4, &thnd, 1);
#else
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svtrace1", 2048, &arg2, 4, &thnd, 0);
#endif
    printf("Created task %p\n", thnd);

    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svsync0", 2048, &arg3, 3, &thnd, 0);
    printf("Created task %p\n", thnd);
#if CONFIG_FREERTOS_UNICORE == 0
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svsync1", 2048, &arg4, 5, &thnd, 1);
#else
    xTaskCreatePinnedToCore(esp_sysviewtrace_test_task, "svsync1", 2048, &arg4, 5, &thnd, 0);
#endif
    printf("Created task %p\n", thnd);

    xSemaphoreTake(arg1.done, portMAX_DELAY);
    vSemaphoreDelete(arg1.done);
    xSemaphoreTake(arg2.done, portMAX_DELAY);
    vSemaphoreDelete(arg2.done);
    xSemaphoreTake(arg3.done, portMAX_DELAY);
    vSemaphoreDelete(arg3.done);
    xSemaphoreTake(arg4.done, portMAX_DELAY);
    vSemaphoreDelete(arg4.done);
    vSemaphoreDelete(test_sync);
    TEST_ESP_OK(gptimer_stop(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_disable(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_del_timer(tim_arg1.gptimer));
    TEST_ESP_OK(gptimer_stop(tim_arg2.gptimer));
    TEST_ESP_OK(gptimer_disable(tim_arg2.gptimer));
    TEST_ESP_OK(gptimer_del_timer(tim_arg2.gptimer));
}
