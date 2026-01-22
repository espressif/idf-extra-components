
#include <stdio.h>
#include "esp_cli_commands.h"
#include "command_utils.h"


// Context for the command (must be known at compile time for static registration)
static int hello_cmd_ctx = 0;

// Handler function signature must match: int (*)(void *, esp_cli_commands_exec_arg_t *, int, char **)
static int hello_cmd_handler(void *ctx, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)ctx;
    (void)argc;
    (void)argv;
    WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Hello! This is the esp_cli_commands static example.\n");
    return 0;
}

// Hint callback signature: const char *(*)(void *)
static const char *hello_cmd_hint_cb(void *ctx)
{
    (void)ctx;
    return "[No arguments]";
}

// Glossary callback signature: const char *(*)(void *)
static const char *hello_cmd_glossary_cb(void *ctx)
{
    (void)ctx;
    return "This command prints a hello message for demonstration purposes.";
}

// Static registration of the hello command with all fields
ESP_CLI_COMMAND_REGISTER(
    hello,                  // Command name
    example,                // Command group
    "Prints a hello message", // Help string
    hello_cmd_handler,      // Handler function
    &hello_cmd_ctx,         // Context pointer (must be address of static object) (optional argument)
    hello_cmd_hint_cb,      // Hint callback (optional argument)
    hello_cmd_glossary_cb   // Glossary callback (optional argument)
);

void app_main(void)
{
    printf("esp_cli_commands static_registration example started.\n");

    esp_cli_commands_exec_arg_t cmd_args = {
        .out_fd = STDOUT_FILENO,
        .write_func = write,
        .dynamic_ctx = NULL
    };

    // Print help output for all commands
    int ret = -1;
    esp_err_t err = esp_cli_commands_execute("help", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'help' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'help' command, error: %d\n", err);
    }

    // Find the 'hello' command by name
    esp_cli_command_t *cmd = esp_cli_commands_find_command(NULL, "hello");
    if (cmd) {
        printf("Found command: %s\n", cmd->name);
    } else {
        printf("Command 'hello' not found!\n");
    }

    // Execute the 'hello' command programmatically
    ret = -1;
    err = esp_cli_commands_execute("hello", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'hello' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'hello' command, error: %d\n", err);
    }

    // Get hint for the 'hello' command
    int color = 0;
    bool bold = false;
    const char *hint = esp_cli_commands_get_hint(NULL, "hello", &color, &bold);
    printf("Hint for 'hello': %s (color: %d, bold: %d)\n", hint ? hint : "none", color, bold);

    // Get glossary for the 'hello' command
    const char *glossary = esp_cli_commands_get_glossary(NULL, "hello");
    printf("Glossary for 'hello': %s\n", glossary ? glossary : "none");

    printf("end of example\n");
}
