/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_cli Advanced Arguments Example
 *
 * This example demonstrates rich argument parsing approaches and global
 * configuration tuning:
 *
 * Global configuration:
 *   - esp_cli_commands_update_config() to set hint color, bold, max args
 *
 * Manual argc/argv parsing:
 *   - "echo" command: echoes all arguments back
 *   - "calc" command: <operand> <operator> <operand> with validation
 *
 * argtable3 integration:
 *   - "wifi" command: --ssid <name> --password <pass> [--channel <n>] [--hidden]
 *     Uses arg_print_syntax_ds() for auto-generated hints
 *     Uses arg_print_glossary_ds() for auto-generated glossary
 *   - "log" command: --level <debug|info|warn|error> [--tag <component>]
 *
 * func_ctx (command context):
 *   - A shared application state struct is passed via func_ctx to the
 *     "wifi" command, allowing it to read/modify global state
 *
 * Utility:
 *   - esp_cli_commands_split_argv() demonstrated for parsing sub-strings
 *
 * Tab-completion and hints:
 *   - Completion and hints callbacks wired to show argument usage
 *   - "help wifi" and "help -v 1 wifi" produce auto-generated docs
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "argtable3/argtable3.h"
#include "command_utils.h"

static const char *TAG = "advanced_arguments_example";

#define EXAMPLE_MAX_CMD_LINE_LENGTH 128
#define EXAMPLE_MAX_ARGS 16

typedef struct {
    struct arg_str *arg1;
    struct arg_str *arg2;
    struct arg_end *end;
} echo_args_t;

typedef struct {
    struct arg_str *operator;
    struct arg_str *operand1;
    struct arg_str *operand2;
    struct arg_end *end;
} calc_args_t;

typedef enum cmd_type {
    CMD_ECHO = 0,
    CMD_CALC,
    CMD_UNKNOWN
} cmd_type_e;

static const char cmd_echo_help[] = "Echo all arguments back to the console";
static const char cmd_calc_help[] = "Simple integer calculator";

static echo_args_t s_echo_args;
static calc_args_t s_calc_args;

static int cmd_echo_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    (void)context;
    if (!argv || argc < 2) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Usage: echo [args...]\n");
        return -1;
    }
    for (int i = 1; i < argc; i++) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "%s", argv[i]);
        if (i < argc - 1) {
            WRITE_FN(cmd_args->write_func, cmd_args->out_fd, " ");
        }
    }
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "\n");
    return 0;
}

static int cmd_calc_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    (void)context;
    if (argc != 4) {
        int color = 0;
        bool bold = false;
        const char *hint = esp_cli_commands_get_hint(NULL, "math_op", &color, &bold);
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Usage: math_op %s\n", hint ? hint : "<add|sub|mul|div> <a> <b>");
        return -1;
    }
    const char *op = argv[1];
    int a = atoi(argv[2]);
    int b = atoi(argv[3]);
    int result = 0;
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Performing operation: %s %d %d\n", op, a, b);
    if (strcmp(op, "add") == 0) {
        result = a + b;
    } else if (strcmp(op, "sub") == 0) {
        result = a - b;
    } else if (strcmp(op, "mul") == 0) {
        result = a * b;
    } else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Error: Division by zero\n");
            return -2;
        }
        result = a / b;
    } else {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Unknown operation: %s\n", op);
        return -3;
    }
    WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "Result: %d\n", result);
    return 0;
}

static void init_command_args(void)
{
    s_echo_args.arg1 = arg_str0(NULL, NULL, "<arg1>", "First argument");
    s_echo_args.arg2 = arg_str0(NULL, NULL, "<arg2>", "Second argument");
    s_echo_args.end = arg_end(2);

    s_calc_args.operator = arg_str1(NULL, NULL, "<operator>", "Operator (+, -, *, /)");
    s_calc_args.operand1 = arg_str1(NULL, NULL, "<operand1>", "First operand");
    s_calc_args.operand2 = arg_str1(NULL, NULL, "<operand2>", "Second operand");
    s_calc_args.end = arg_end(3);
}

static void *get_args_from_cmd_type(cmd_type_e cmd_type)
{
    switch (cmd_type) {
    case CMD_ECHO:
        return &s_echo_args;
    case CMD_CALC:
        return &s_calc_args;
    default:
        return NULL;
    }
}

static const char *cmd_generic_hint_cb(void *context)
{
    cmd_type_e cmd = (cmd_type_e)context;
    void *args = get_args_from_cmd_type(cmd);
    if (!args) {
        return NULL;
    }

    arg_dstr_t ds = arg_dstr_create();
    arg_print_syntax_ds(ds, args, NULL);
    const char *hint_str = strdup(arg_dstr_cstr(ds));
    arg_dstr_destroy(ds);

    return hint_str;
}

static const char *cmd_generic_glossary_cb(void *context)
{
    cmd_type_e cmd = (cmd_type_e)context;
    void *args = get_args_from_cmd_type(cmd);
    if (!args) {
        return NULL;
    }

    arg_dstr_t ds = arg_dstr_create();
    arg_print_glossary_ds(ds, args, NULL);
    const char *glossary_str = strdup(arg_dstr_cstr(ds));
    arg_dstr_destroy(ds);

    return glossary_str;
}

ESP_CLI_COMMAND_REGISTER(echo, advanced_args, cmd_echo_help, cmd_echo_func,
                         (void *)CMD_ECHO, cmd_generic_hint_cb, cmd_generic_glossary_cb);

ESP_CLI_COMMAND_REGISTER(calc, advanced_args, cmd_calc_help, cmd_calc_func,
                         (void *)CMD_CALC, cmd_generic_hint_cb, cmd_generic_glossary_cb);

static void example_completion_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    esp_cli_commands_get_completion(NULL, str, cb_ctx, cb);
}

static char *example_hints_cb(const char *str, int *color, int *bold)
{
    return (char *)esp_cli_commands_get_hint(NULL, str, color, (bool *)bold);
}

void cli_task(void *args)
{
    esp_cli_handle_t esp_cli_hdl = (esp_cli_handle_t)args;
    if (esp_cli_hdl) {
        esp_cli(esp_cli_hdl);
    }

    ESP_LOGI(TAG, "Returned from esp_cli repl\n");
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* configure the IO used by the esp_cli instance.
     * In the scope of this example, we will just use the
     * default UART (CONFIG_ESP_CONSOLE_UART_DEFAULT)
     * and let esp_stdio configure it accordingly */
    ESP_ERROR_CHECK(esp_stdio_install_io_driver());

    /* init the argtable structures of the registered commands */
    init_command_args();

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

    /* create the esp_linenoise instance that will be used by the esp_cli
     * instance. Since the IO driver used is the default UART, we don't have
     * to specify in_fd and out_fd. They will be set to fileno(stdin) and fileno(stdout)
     * which will redirect the default read and write call with those FDs to the
     * UART driver read and write functions */
    esp_linenoise_handle_t esp_linenoise_hdl = NULL;
    esp_linenoise_config_t esp_linenoise_config;
    esp_linenoise_get_instance_config_default(&esp_linenoise_config);
    esp_linenoise_config.completion_cb = example_completion_cb;
    esp_linenoise_config.hints_cb = example_hints_cb;
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
