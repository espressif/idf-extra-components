#include <stdio.h>
#include <string.h>
#include "esp_cli_commands.h"
#include "command_utils.h"

static int debug_cmd_handler(void *ctx, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)ctx;
    WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Debug info: CLI is running.\n");
    return 0;
}

static const char *debug_cmd_hint_cb(void *ctx)
{
    (void)ctx;
    return "[No arguments]";
}

static const char *debug_cmd_glossary_cb(void *ctx)
{
    (void)ctx;
    return "Prints debug information.";
}

// Unregister command context

static int unregister_cmd_handler(void *ctx, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)ctx;
    if (argc != 2) {
        int color = 0;
        bool bold = false;
        const char *hint = esp_cli_commands_get_hint(NULL, "unregister", &color, &bold);
        WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Usage: unregister <command> %s\n", hint ? hint : "<command>");
        return -1;
    }
    const char *cmd_name = argv[1];
    esp_err_t err = esp_cli_commands_unregister_cmd(cmd_name);
    if (err == ESP_OK) {
        WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Command '%s' unregistered successfully.\n", cmd_name);
        // If unregistering itself, print a message
        if (strcmp(cmd_name, "unregister") == 0) {
            WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "'unregister' command has removed itself.\n");
        }
    } else {
        WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Failed to unregister command '%s', error: %d\n", cmd_name, err);
    }
    return err;
}

static const char *unregister_cmd_hint_cb(void *ctx)
{
    (void)ctx;
    return "<command>";
}

static const char *unregister_cmd_glossary_cb(void *ctx)
{
    (void)ctx;
    return "Unregisters a dynamically registered command, including itself.";
}

void app_main(void)
{
    printf("esp_cli_commands dynamic_registration example started.\n");

    esp_cli_commands_exec_arg_t cmd_args = {
        .out_fd = STDOUT_FILENO,
        .write_func = write,
        .dynamic_ctx = NULL
    };

    int ret = -1;
    esp_err_t err = ESP_FAIL;

    // Dynamically register debug command
    esp_cli_command_t debug_cmd = {
        .name = "debug",
        .group = "example",
        .help = "Prints debug information",
        .func = debug_cmd_handler,
        .func_ctx = NULL,
        .hint_cb = debug_cmd_hint_cb,
        .glossary_cb = debug_cmd_glossary_cb
    };
    ESP_ERROR_CHECK(esp_cli_commands_register_cmd(&debug_cmd));

    // Dynamically register unregister commandF
    esp_cli_command_t unregister_cmd = {
        .name = "unregister",
        .group = "example",
        .help = "Unregisters a command by name",
        .func = unregister_cmd_handler,
        .func_ctx = NULL,
        .hint_cb = unregister_cmd_hint_cb,
        .glossary_cb = unregister_cmd_glossary_cb
    };
    ESP_ERROR_CHECK(esp_cli_commands_register_cmd(&unregister_cmd));

    // Show that debug and unregister commands are available
    ret = -1;
    err = esp_cli_commands_execute("help", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'help' command executed successfully after dynamic registration, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'help' command after dynamic registration, error: %d\n", err);
    }

    // Execute debug command
    ret = -1;
    err = esp_cli_commands_execute("debug", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'debug' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'debug' command, error: %d\n", err);
    }

    // Unregister debug command using unregister command
    ret = -1;
    err = esp_cli_commands_execute("unregister debug", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'unregister debug' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'unregister debug' command, error: %d\n", err);
    }

    // Unregister itself
    ret = -1;
    err = esp_cli_commands_execute("unregister unregister", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'unregister unregister' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'unregister unregister' command, error: %d\n", err);
    }

    // Show that debug and unregister commands are no longer registered
    ret = -1;
    err = esp_cli_commands_execute("help", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'help' command executed successfully after dynamic registration, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'help' command after dynamic registration, error: %d\n", err);
    }

    printf("end of example\n");
}
