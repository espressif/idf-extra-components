/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_cli Lifecycle Hooks Example
 *
 * This example demonstrates all five esp_cli lifecycle callbacks and
 * filesystem-based history persistence:
 *
 * Callbacks demonstrated:
 *   - on_enter:       Called once when esp_cli() enters the REPL loop.
 *                     Used here to log session start and change the prompt.
 *   - pre_executor:   Called before each command execution.
 *                     Used here to log the raw command line and demonstrate
 *                     rejecting a "forbidden" command by returning an error.
 *   - post_executor:  Called after each command execution.
 *                     Used here to log the result and count executed commands.
 *   - on_stop:        Called when esp_cli_stop() is invoked.
 *                     Used here to log that the CLI is stopping.
 *   - on_exit:        Called when esp_cli() returns, just before the function exits.
 *                     Used here to log final session statistics.
 *
 * History persistence:
 *   - Uses SPIFFS to store command history across reboots.
 *   - esp_linenoise_history_load() restores history at startup.
 *   - history_save_path in esp_cli_config_t triggers automatic save after
 *     each command.
 *
 * Enable CONFIG_ESP_CLI_HAS_QUIT_CMD=y to get the "quit" command.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_stdio.h"
#include "esp_cli.h"
#include "esp_cli_commands.h"
#include "esp_linenoise.h"
#include "command_utils.h"

static const char *TAG = "lifecycle_hooks_example";

#define EXAMPLE_MAX_CMD_LINE_LENGTH 128
#define HISTORY_FILE_PATH           "/spiffs/cli_history.txt"
#define SESSION_COUNT_FILE          "/spiffs/session_count.txt"

/* This structure is shared across all callbacks to track session state.
 * A pointer to this is passed as the `ctx` field of each callback config. */
typedef struct {
    esp_linenoise_handle_t esp_linenoise_hdl; /* Linenoise handle (for prompt change, etc.) */
    int commands_executed; /* Running count of executed commands */
    int session_number; /* Monotonically increasing session counter */
} lifecycle_ctx_t;

/* The context pointer will be set after ctx is initialized.
 * Use a static pointer so we can reference it in the registration macro. */
static lifecycle_ctx_t *s_status_ctx = NULL;

/* help string for the status command */
static const char cmd_status_help[] = "Print system status";

/* Load the session counter from SPIFFS.
 * Returns the counter value, or 0 if the file doesn't exist or can't be read. */
static int load_session_counter(void)
{
    FILE *f = fopen(SESSION_COUNT_FILE, "r");
    if (!f) {
        return 0; /* File doesn't exist yet */
    }

    int count = 0;
    if (fscanf(f, "%d", &count) != 1) {
        count = 0; /* Failed to read */
    }
    fclose(f);
    return count;
}

/* Save the session counter to SPIFFS.
 * Returns ESP_OK on success, ESP_FAIL on error. */
static esp_err_t save_session_counter(int count)
{
    FILE *f = fopen(SESSION_COUNT_FILE, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", SESSION_COUNT_FILE);
        return ESP_FAIL;
    }

    if (fprintf(f, "%d\n", count) < 0) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to write session counter");
        return ESP_FAIL;
    }

    fclose(f);
    return ESP_OK;
}

/* on_enter callback — called once when esp_cli() enters the REPL loop.
 * Loads the persistent session counter from SPIFFS, increments it, saves it back,
 * resets the command counter, and dynamically changes the prompt to "session-N> ".
 * This demonstrates persistent storage across reboots to track total boot count. */
static void on_enter_cb(void *ctx, esp_cli_handle_t handle)
{
    lifecycle_ctx_t *lctx = (lifecycle_ctx_t *)ctx;
    (void)handle;

    /* Load, increment, and save the persistent session counter */
    lctx->session_number = load_session_counter() + 1;
    save_session_counter(lctx->session_number);
    lctx->commands_executed = 0;

    /* Build a dynamic prompt that includes the session number */
    static char prompt[64];
    snprintf(prompt, sizeof(prompt), "session-%d> ", lctx->session_number);
    esp_linenoise_set_prompt(lctx->esp_linenoise_hdl, prompt);

    ESP_LOGI(TAG, "on_enter: CLI session %d started (boot count)", lctx->session_number);
}

/* pre_executor callback — called before each command execution.
 * Logs the raw command line. Demonstrates command rejection by
 * returning ESP_FAIL for commands starting with "secret". */
static esp_err_t pre_executor_cb(void *ctx, const char *buf, esp_err_t reader_ret_val)
{
    (void)ctx;
    (void)reader_ret_val;

    ESP_LOGI(TAG, "pre_executor: '%s'", buf ? buf : "(null)");

    /* Demonstrate command rejection: block any command starting with "secret" */
    if (buf && strncmp(buf, "secret", 6) == 0) {
        ESP_LOGW(TAG, "pre_executor: command '%s' is forbidden!", buf);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* post_executor callback — called after each command execution.
 * Increments the command counter and logs the command name,
 * execution result, and command return value. */
static esp_err_t post_executor_cb(void *ctx, const char *buf, esp_err_t executor_ret_val, int cmd_ret_val)
{
    lifecycle_ctx_t *lctx = (lifecycle_ctx_t *)ctx;

    lctx->commands_executed++;

    ESP_LOGI(TAG, "post_executor: cmd='%s' exec_ret=%s cmd_ret=%d (total: %d)",
             buf ? buf : "(null)",
             esp_err_to_name(executor_ret_val),
             cmd_ret_val,
             lctx->commands_executed);

    return ESP_OK;
}

/* on_stop callback — called when esp_cli_stop() is invoked.
 * Logs that a stop was requested. */
static void on_stop_cb(void *ctx, esp_cli_handle_t handle)
{
    (void)ctx;
    (void)handle;

    ESP_LOGI(TAG, "on_stop: CLI stop requested");
}

/* on_exit callback — called just before esp_cli() returns.
 * Logs final session statistics. */
static void on_exit_cb(void *ctx, esp_cli_handle_t handle)
{
    lifecycle_ctx_t *lctx = (lifecycle_ctx_t *)ctx;
    (void)handle;

    ESP_LOGI(TAG, "on_exit: Session %d ended. Commands executed: %d",
             lctx->session_number, lctx->commands_executed);
}

/* "status" command — prints session info so the user can observe
 * the pre/post executor callbacks in action. */
static int cmd_status_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    /* context from static registration is NULL; use the global pointer instead */
    lifecycle_ctx_t *ctx = s_status_ctx;
    (void)context;
    (void)argc;
    (void)argv;

    if (!ctx) {
        WRITE_FN(cmd_args->write_func, cmd_args->out_fd, "System not initialized yet\n");
        return 1;
    }

    WRITE_FN(cmd_args->write_func, cmd_args->out_fd,
             "System OK | Session: %d | Commands executed: %d\n",
             ctx->session_number, ctx->commands_executed);
    return 0;
}

ESP_CLI_COMMAND_REGISTER(status,
                         lifecycle_example,
                         cmd_status_help,
                         cmd_status_func,
                         NULL, /* context patched at runtime via s_status_ctx */
                         NULL,
                         NULL);

static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS for history persistence");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

static void cli_task(void *arg)
{
    esp_cli_handle_t cli_hdl = (esp_cli_handle_t)arg;

    esp_cli(cli_hdl);

    ESP_LOGI(TAG, "CLI task exiting");
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* Console I/O is automatically initialized by the esp_stdio component */
    ESP_ERROR_CHECK(esp_stdio_install_io_driver());

    /* Initialize SPIFFS for history persistence */
    if (init_spiffs() != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS init failed — history will not be persisted");
    }

    /* Create an esp_linenoise instance */
    esp_linenoise_handle_t esp_linenoise_hdl = NULL;
    esp_linenoise_config_t esp_linenoise_config;
    esp_linenoise_get_instance_config_default(&esp_linenoise_config);
    esp_linenoise_config.prompt = "> "; /* Will be overridden dynamically in on_enter_cb */
    esp_linenoise_config.max_cmd_line_length = EXAMPLE_MAX_CMD_LINE_LENGTH;
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&esp_linenoise_config, &esp_linenoise_hdl));
    if (!esp_linenoise_hdl) {
        ESP_LOGE(TAG, "Failed to create esp_linenoise instance");
        return;
    }

    /* Load command history from SPIFFS (if file exists) */
    esp_linenoise_history_set_max_len(esp_linenoise_hdl, 20);
    esp_linenoise_history_load(esp_linenoise_hdl, HISTORY_FILE_PATH);

    /* Prepare the shared callback context */
    static lifecycle_ctx_t ctx = {0};
    ctx.esp_linenoise_hdl = esp_linenoise_hdl;
    ctx.commands_executed = 0;
    ctx.session_number = load_session_counter(); /* Load persisted boot count */
    s_status_ctx = &ctx;

    /* Create the esp_cli instance with all callbacks */
    esp_cli_handle_t cli_hdl = NULL;
    esp_cli_config_t cli_config = {
        .linenoise_handle    = esp_linenoise_hdl,
        .command_set_handle  = NULL, /* all commands */
        .max_cmd_line_size   = EXAMPLE_MAX_CMD_LINE_LENGTH,
        .history_save_path   = HISTORY_FILE_PATH,
        .on_enter            = { .func = on_enter_cb,      .ctx = &ctx },
        .pre_executor        = { .func = pre_executor_cb,  .ctx = &ctx },
        .post_executor       = { .func = post_executor_cb, .ctx = &ctx },
        .on_stop             = { .func = on_stop_cb,       .ctx = &ctx },
        .on_exit             = { .func = on_exit_cb,       .ctx = &ctx },
    };
    ESP_ERROR_CHECK(esp_cli_create(&cli_config, &cli_hdl));

    /* Create task and start the REPL */
    xTaskCreate(cli_task, "cli_task", 4096, cli_hdl, 5, NULL);
    ESP_ERROR_CHECK(esp_cli_start(cli_hdl));

    ESP_LOGI(TAG, "CLI with lifecycle hooks started. Type 'help' or 'quit'.");
}
