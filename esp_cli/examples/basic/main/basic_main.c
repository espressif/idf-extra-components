/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_cli Basic Example
 *
 * This example demonstrates the fundamental usage of the esp_cli component:
 * - Initializing I/O for console interaction
 * - Creating an esp_linenoise instance with tab-completion and hints
 * - Registering static commands via ESP_CLI_COMMAND_REGISTER
 * - Creating and running an esp_cli REPL instance in a FreeRTOS task
 * - Using the built-in "help" and "quit" commands
 * - Proper lifecycle: create → task → start → quit → destroy
 *
 * Enable CONFIG_ESP_CLI_HAS_QUIT_CMD=y in sdkconfig to get the "quit" command.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_stdio.h"
#include "esp_cli.h"
#include "esp_cli_commands.h"
#include "esp_linenoise.h"
#include "command_utils.h"

static const char *TAG = "basic_example";

#define EXAMPLE_MAX_CMD_LINE_LENGTH 128

static int cmd_hello_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    if (argc > 1) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Hello, %s!\n", argv[1]);
    } else {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Hello, World!\n");
    }
    return 0;
}

static const char *cmd_hello_hint(void *context)
{
    return "[name]";
}

static const char cmd_hello_help[] = "Print a greeting message";

ESP_CLI_COMMAND_REGISTER(hello,
                         basic_example,
                         cmd_hello_help,
                         cmd_hello_func,
                         NULL,
                         cmd_hello_hint,
                         NULL);

/*
 * This task runs the esp_cli REPL loop. It blocks in esp_cli() until
 * esp_cli_stop() is called (e.g. via the "quit" command).
 */
static void cli_task(void *arg)
{
    esp_cli_handle_t cli_hdl = (esp_cli_handle_t)arg;

    /* esp_cli() blocks here until the REPL exits */
    esp_cli(cli_hdl);

    ESP_LOGI(TAG, "CLI task exiting");
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* configure the IO used by the esp_cli instance.
     * In the scope of this example, we will just use the
     * default UART (CONFIG_ESP_CONSOLE_UART_DEFAULT)
     * and let esp_stdio configure it accordingly */
    ESP_ERROR_CHECK(esp_stdio_install_io_driver());

    /* create the esp_linenoise instance that will be used by the esp_cli
     * instance. Since the IO driver used is the default UART, we don't have
     * to specify in_fd and out_fd. They will be set to fileno(stdin) and fileno(stdout)
     * which will redirect the default read and write call with those FDs to the
     * UART driver read and write functions */
    esp_linenoise_handle_t esp_linenoise_hdl = NULL;
    esp_linenoise_config_t esp_linenoise_config;
    esp_linenoise_get_instance_config_default(&esp_linenoise_config);
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&esp_linenoise_config, &esp_linenoise_hdl));
    if (!esp_linenoise_hdl) {
        ESP_LOGE(TAG, "Failed to create esp_linenoise instance\n");
        return;
    }

    /* create an esp_cli instance */
    esp_cli_handle_t esp_cli_hdl = NULL;
    esp_cli_config_t esp_cli_config = {
        esp_linenoise_hdl,
        NULL, /* this example does not require a command set handle to be specified */
        EXAMPLE_MAX_CMD_LINE_LENGTH,
        NULL, /* this example does not require a file path to store the history */
        { NULL, NULL }, /* this example does not require this callback to be implemented */
        { NULL, NULL }, /* this example does not require this callback to be implemented */
        { NULL, NULL }, /* this example does not require this callback to be implemented */
        { NULL, NULL }, /* this example does not require this callback to be implemented */
        { NULL, NULL }, /* this example does not require this callback to be implemented */
    };
    ESP_ERROR_CHECK(esp_cli_create(&esp_cli_config, &esp_cli_hdl));

    xTaskCreate(cli_task, "cli_task", 4096, esp_cli_hdl, 5, NULL);

    /* start the CLI repl loop */
    ESP_ERROR_CHECK(esp_cli_start(esp_cli_hdl));
}
