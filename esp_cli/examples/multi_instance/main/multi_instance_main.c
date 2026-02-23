/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_cli Multi-Instance Example
 *
 * This example demonstrates running two independent esp_cli instances
 * simultaneously on different I/O interfaces:
 *
 * Multiple instances:
 *   - Instance 1 ("user"): runs on UART with prompt "uart> "
 *   - Instance 2 ("admin"): runs on USB Serial JTAG with prompt "jtag> "
 *   - Each has its own FreeRTOS task, esp_linenoise handle, and command set
 *
 * Per-instance command sets:
 *   - Instance 1 ("user"): only "common" group commands (help, status, stop_admin)
 *   - Instance 2 ("admin"): all commands including privileged ones (reboot, config)
 *
 * Cross-task stop:
 *   - Instance 1 has a "stop_admin" command that calls esp_cli_stop() on
 *     Instance 2 from a different task, demonstrating thread-safe stop
 *
 * Command output:
 *   - Commands use cmd_args->write_func(cmd_args->out_fd, ...) so that
 *     output goes to the correct interface regardless of which instance
 *     is executing the command
 *
 * Requirements:
 *   - CONFIG_ESP_LINENOISE_MAX_INSTANCE_NB >= 2
 *   - A board with both UART and USB Serial JTAG (e.g., ESP32-S3)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_cli.h"
#include "esp_cli_commands.h"
#include "esp_linenoise.h"
#include "cli_example_io.h"
#include "command_utils.h"

static const char *TAG = "multi_instance_example";

#define EXAMPLE_MAX_CMD_LINE_LENGTH 64
#define EXAMPLE_MAX_ARGS 8
#define ANSI_BOLD          "\033[1m"
#define ANSI_COLOR_BLUE    "\033[95m"
#define ANSI_COLOR_RESET   "\033[0m"
#define CLI_UART_NUM 1
#define CLI_UART_TX 4
#define CLI_UART_RX 5

/* Handle to the admin instance — needed by the "stop_admin" command */
static esp_cli_handle_t s_admin_cli_hdl = NULL;

static void admin_on_stop_cb(void *ctx, esp_cli_handle_t handle)
{
    (void)ctx;
    (void)handle;
    ESP_LOGI(TAG, "Admin CLI stop requested");
}

/* User instance: only searches the "common" command set */
static esp_cli_command_set_handle_t s_user_cmd_set = NULL;

static void user_completion_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    esp_cli_commands_get_completion(s_user_cmd_set, str, cb_ctx, cb);
}

static char *user_hints_cb(const char *str, int *color, int *bold)
{
    return (char *)esp_cli_commands_get_hint(s_user_cmd_set, str, color, (bool *)bold);
}

/* Admin instance: searches all registered commands */
static void admin_completion_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    esp_cli_commands_get_completion(NULL, str, cb_ctx, cb);
}

static char *admin_hints_cb(const char *str, int *color, int *bold)
{
    return (char *)esp_cli_commands_get_hint(NULL, str, color, (bool *)bold);
}

static int cmd_status_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
             "System status: OK | Free heap: %lu bytes\n",
             (unsigned long)esp_get_free_heap_size());
    return 0;
}

static const char cmd_status_help[] = "Print system status";

ESP_CLI_COMMAND_REGISTER(status,
                         common,
                         cmd_status_help,
                         cmd_status_func,
                         NULL,
                         NULL,
                         NULL);

static int cmd_stop_admin_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    if (s_admin_cli_hdl == NULL) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Admin instance not available\n");
        return 1;
    }

    esp_err_t ret = esp_cli_stop(s_admin_cli_hdl);
    if (ret == ESP_OK) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Admin CLI stop signal sent\n");
    } else {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
                 "Failed to stop admin CLI: %s\n", esp_err_to_name(ret));
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static const char cmd_stop_admin_help[] = "Stop the admin CLI instance (cross-task stop demo)";

ESP_CLI_COMMAND_REGISTER(stop_admin,
                         common,
                         cmd_stop_admin_help,
                         cmd_stop_admin_func,
                         NULL,
                         NULL,
                         NULL);

static int cmd_config_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    if (argc < 2) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
                 "Usage: config <key> [value]\n");
        return 1;
    }
    if (argc == 2) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
                 "%s = (not set)\n", argv[1]);
    } else {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
                 "%s = %s (updated)\n", argv[1], argv[2]);
    }
    return 0;
}

static const char *cmd_config_hint(void *context)
{
    return "<key> [value]";
}

static const char cmd_config_help[] = "Get or set configuration values (admin only)";

ESP_CLI_COMMAND_REGISTER(config,
                         admin,
                         cmd_config_help,
                         cmd_config_func,
                         NULL,
                         cmd_config_hint,
                         NULL);

static int cmd_reboot_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
             "Reboot requested (not actually rebooting in this demo)\n");
    return 0;
}

static const char cmd_reboot_help[] = "Reboot the system (admin only)";

ESP_CLI_COMMAND_REGISTER(reboot,
                         admin,
                         cmd_reboot_help,
                         cmd_reboot_func,
                         NULL,
                         NULL,
                         NULL);

static void user_cli_task(void *arg)
{
    esp_cli_handle_t cli_hdl = (esp_cli_handle_t)arg;
    esp_cli(cli_hdl);
    ESP_LOGI(TAG, "User CLI task exiting");
    vTaskDelete(NULL);
}

static void admin_cli_task(void *arg)
{
    esp_cli_handle_t cli_hdl = (esp_cli_handle_t)arg;
    esp_cli(cli_hdl);
    ESP_LOGI(TAG, "Admin CLI task exiting");
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* init the uart driver for the first instance of esp_cli */
    int uart_in_fd = -1, uart_out_fd = -1;
    ESP_ERROR_CHECK(cli_example_init_uart(CLI_UART_NUM, CLI_UART_TX, CLI_UART_RX,
                                          &uart_in_fd, &uart_out_fd));

    /* init usb serial jtag for the second instance of esp_cli */
    int usj_in_fd = -1, usj_out_fd = -1;
    ESP_ERROR_CHECK(cli_example_init_usb_serial_jtag(&usj_in_fd, &usj_out_fd));

    /* update the esp_cli_commands configuration if the default config
     * is not suitable */
    esp_cli_commands_config_t cmd_config = {
        .hint_color = 36,
        .hint_bold = true,
        .max_cmdline_args = EXAMPLE_MAX_ARGS,
        .max_cmdline_length = EXAMPLE_MAX_CMD_LINE_LENGTH,
        .heap_caps_used = MALLOC_CAP_DEFAULT,
    };
    ESP_ERROR_CHECK(esp_cli_commands_update_config(&cmd_config));

    /* init esp_linenoise instance for the user (UART) */
    esp_linenoise_handle_t user_linenoise_hdl = NULL;
    esp_linenoise_config_t user_ln_config;
    esp_linenoise_get_instance_config_default(&user_ln_config);
    user_ln_config.prompt = ANSI_BOLD ANSI_COLOR_BLUE "user" ANSI_COLOR_RESET ">";
    user_ln_config.in_fd = uart_in_fd;
    user_ln_config.out_fd = uart_out_fd;
    user_ln_config.max_cmd_line_length = EXAMPLE_MAX_CMD_LINE_LENGTH;
    user_ln_config.completion_cb = user_completion_cb;
    user_ln_config.hints_cb = user_hints_cb;
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&user_ln_config, &user_linenoise_hdl));

    /* init esp_linenoise instance for the admin (JTAG) */
    esp_linenoise_handle_t admin_linenoise_hdl = NULL;
    esp_linenoise_config_t admin_ln_config;
    esp_linenoise_get_instance_config_default(&admin_ln_config);
    admin_ln_config.prompt = ANSI_BOLD ANSI_COLOR_BLUE "admin" ANSI_COLOR_RESET ">";
    admin_ln_config.in_fd = usj_in_fd;
    admin_ln_config.out_fd = usj_out_fd;
    admin_ln_config.max_cmd_line_length = EXAMPLE_MAX_CMD_LINE_LENGTH;
    admin_ln_config.completion_cb = admin_completion_cb;
    admin_ln_config.hints_cb = admin_hints_cb;
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&admin_ln_config, &admin_linenoise_hdl));

    /* User: only "common" group commands (status, stop_admin, help) */
    const char *user_groups[] = { "common" };
    s_user_cmd_set = ESP_CLI_COMMANDS_CREATE_CMD_SET(user_groups, ESP_CLI_COMMAND_FIELD_ACCESSOR(group));

    /* Admin: all commands (pass NULL as command_set_handle) */

    /* Create the user (UART) esp_cli instance */
    esp_cli_handle_t user_cli_hdl = NULL;
    esp_cli_config_t user_cli_config = {
        .linenoise_handle    = user_linenoise_hdl,
        .command_set_handle  = s_user_cmd_set,
        .max_cmd_line_size   = EXAMPLE_MAX_CMD_LINE_LENGTH,
        .history_save_path   = NULL,
        .on_enter            = { NULL, NULL },
        .pre_executor        = { NULL, NULL },
        .post_executor       = { NULL, NULL },
        .on_stop             = { NULL, NULL },
        .on_exit             = { NULL, NULL },
    };
    ESP_ERROR_CHECK(esp_cli_create(&user_cli_config, &user_cli_hdl));

    /* Create the admin (JTAG) esp_cli instance */
    esp_cli_handle_t admin_cli_hdl = NULL;
    esp_cli_config_t admin_cli_config = {
        .linenoise_handle    = admin_linenoise_hdl,
        .command_set_handle  = NULL, /* all commands visible */
        .max_cmd_line_size   = EXAMPLE_MAX_CMD_LINE_LENGTH,
        .history_save_path   = NULL,
        .on_enter            = { NULL, NULL },
        .pre_executor        = { NULL, NULL },
        .post_executor       = { NULL, NULL },
        .on_stop             = { .func = admin_on_stop_cb, .ctx = NULL },
        .on_exit             = { NULL, NULL },
    };
    ESP_ERROR_CHECK(esp_cli_create(&admin_cli_config, &admin_cli_hdl));
    s_admin_cli_hdl = admin_cli_hdl;

    /* Create tasks and start both instances */
    xTaskCreate(user_cli_task, "user_cli", 4096, user_cli_hdl, 5, NULL);
    xTaskCreate(admin_cli_task, "admin_cli", 4096, admin_cli_hdl, 5, NULL);

    ESP_ERROR_CHECK(esp_cli_start(user_cli_hdl));
    ESP_ERROR_CHECK(esp_cli_start(admin_cli_hdl));

    ESP_LOGI(TAG, "Two CLI instances started:");
    ESP_LOGI(TAG, "  UART  (user)  — basic commands only");
    ESP_LOGI(TAG, "  JTAG  (admin) — all commands including reboot, config");
    ESP_LOGI(TAG, "Type 'stop_admin' on UART to stop the admin instance.");
}
