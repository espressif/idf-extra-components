/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "esp_repl.h"
#include "esp_linenoise.h"
#include "esp_commands.h"

esp_err_t test_pre_executor(void *ctx, char *buf, const esp_err_t reader_ret_val)
{
    return ESP_OK;
}

esp_err_t test_post_executor(void *ctx, const char *buf, const esp_err_t executor_ret_val, const int cmd_ret_val)
{
    return ESP_OK;
}

void test_on_stop(void *ctx, esp_repl_instance_handle_t handle)
{
    return;
}

void test_on_exit(void *ctx, esp_repl_instance_handle_t handle)
{
    return;
}

TEST_CASE("esp_repl() called after successful init, with non blocking reader", "[esp_repl]")
{
    esp_linenoise_config_t config = esp_linenoise_get_instance_config_default();
    esp_linenoise_handle_t esp_linenoise_hdl = esp_linenoise_create_instance(&config);

    esp_command_set_handle_t cmd_set_hdl = esp_commands_

                                           esp_commands_dummy_t dummy_esp_commands = {.value = 0x02 };
    esp_repl_config_t config = {
        .max_cmd_line_size = 256,
        .pre_executor = { .func = test_pre_executor, .ctx = NULL },
        .post_executor = { .func = test_post_executor, .ctx = NULL },
        .on_stop = { .func = test_on_stop, .ctx = NULL },
        .on_exit = { .func = test_on_exit, .ctx = NULL }
    };

    esp_repl_instance_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_create(&handle, &config));
    TEST_ASSERT_NOT_NULL(handle);

    xTaskCreate(esp_apptrace_send_uart_tx_task, "app_trace_uart_tx_task", 2500, hw_data, uart_prio, NULL);

}