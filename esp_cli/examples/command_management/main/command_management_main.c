/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_cli Command Management Example
 *
 * This example demonstrates dynamic command registration/unregistration
 * and command set filtering:
 *
 * Static commands and groups:
 *   - Several commands registered via ESP_CLI_COMMAND_REGISTER in two
 *     groups ("system" and "network").
 *
 * Command sets:
 *   - Creating a command set by group using ESP_CLI_COMMAND_FIELD_ACCESSOR(group)
 *   - Creating a command set by name using ESP_CLI_COMMAND_FIELD_ACCESSOR(name)
 *   - Concatenating two command sets with esp_cli_commands_concat_cmd_set()
 *   - Destroying command sets with esp_cli_commands_destroy_cmd_set()
 *   - Passing the command set to esp_cli_config_t to restrict visible commands
 *
 * Dynamic commands:
 *   - A "plugin" command that dynamically registers a new command at runtime
 *     using esp_cli_commands_register_cmd()
 *   - An "unplug" command that removes it with esp_cli_commands_unregister_cmd()
 *   - Using esp_cli_commands_find_command() to check if a command exists
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_stdio.h"
#include "command_utils.h"
#include "esp_cli.h"
#include "esp_cli_commands.h"
#include "esp_linenoise.h"

static const char *TAG = "command_management_example";

#define EXAMPLE_MAX_CMD_LINE_LENGTH 128

static int cmd_custom_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Executing %s\n", __func__);
    return 0;
}

static esp_cli_command_t s_dynamic_cmd = {
    .name = "custom_cmd",
    .group = "plugins",
    .help = "A dynamically registered command",
    .func = cmd_custom_func,
    .func_ctx = NULL,
    .hint_cb = NULL,
    .glossary_cb = NULL,
};

static const char cmd_info_help[] = "Print system information";
static const char cmd_reboot_help[] = "Reboot the system";
static const char cmd_ping_help[] = "Ping a remote host";
static const char cmd_ifconfig_help[] = "Show network interface configuration";
static const char cmd_plugin_help[] = "Dynamically register the 'custom_cmd' command";
static const char cmd_unplug_help[] = "Dynamically unregister the 'custom_cmd' command";

static int cmd_info_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Executing dummy call to %s\n", __func__);
    return 0;
}

static int cmd_reboot_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Executing dummy call to %s\n", __func__);
    return 0;
}

static int cmd_ping_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Executing dummy call to %s\n", __func__);
    return 0;
}

static int cmd_ifconfig_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Executing dummy call to %s\n", __func__);
    return 0;
}

static int cmd_plugin_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    /* the command registers s_dynamic_cmd */
    esp_err_t ret = esp_cli_commands_register_cmd(&s_dynamic_cmd);
    return ret == ESP_OK ? 0 : -1;
}

static int cmd_unplug_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    /* the command unregisters s_dynamic_cmd */
    esp_err_t ret = esp_cli_commands_unregister_cmd("custom_cmd");
    return ret == ESP_OK ? 0 : -1;
}

ESP_CLI_COMMAND_REGISTER(info, system, cmd_info_help, cmd_info_func, NULL, NULL, NULL);
ESP_CLI_COMMAND_REGISTER(reboot, system, cmd_reboot_help, cmd_reboot_func, NULL, NULL, NULL);
ESP_CLI_COMMAND_REGISTER(ping, network, cmd_ping_help, cmd_ping_func, NULL, NULL, NULL);
ESP_CLI_COMMAND_REGISTER(ifconfig, network, cmd_ifconfig_help, cmd_ifconfig_func, NULL, NULL, NULL);
ESP_CLI_COMMAND_REGISTER(plugin, system, cmd_plugin_help, cmd_plugin_func, NULL, NULL, NULL);
ESP_CLI_COMMAND_REGISTER(unplug, system, cmd_unplug_help, cmd_unplug_func, NULL, NULL, NULL);

static void cli_task(void *arg)
{
    esp_cli_handle_t cli_hdl = (esp_cli_handle_t)arg;

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

    /* create a command set based on command name. This will filter out all commands
     * but the ones listed in the set */
    const char *name_cmd_set[] = {
        "plugin",
        "unplug"
    };
    esp_cli_command_set_handle_t cmd_set_name_hdl = ESP_CLI_COMMANDS_CREATE_CMD_SET(name_cmd_set, ESP_CLI_COMMAND_FIELD_ACCESSOR(name));

    /* create a command set by group. This will filter out all commands which group
     * does not appear in the list */
    const char *group_cmd_set[] = {
        "network"
    };
    esp_cli_command_set_handle_t cmd_set_group_hdl = ESP_CLI_COMMANDS_CREATE_CMD_SET(group_cmd_set, ESP_CLI_COMMAND_FIELD_ACCESSOR(group));

    /* concatenate the command sets into one command set regrouping all allowed commands */
    esp_cli_command_set_handle_t esp_command_set_hdl = esp_cli_commands_concat_cmd_set(cmd_set_group_hdl, cmd_set_name_hdl);

    /* create an esp_cli instance */
    esp_cli_handle_t esp_cli_hdl = NULL;
    esp_cli_config_t esp_cli_config = {
        esp_linenoise_hdl,
        esp_command_set_hdl,
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
