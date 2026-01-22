/*
 * Example: Command Set Functionality Demonstration
 *
 * This example creates two commands, each belonging to a different group.
 * It demonstrates the use of command sets for filtering and executing commands.
 *
 * Steps:
 * 1. Create two commands (cmd_a, cmd_b) in groups (group_a, group_b).
 * 2. Create two command sets, each for one group.
 * 3. Execute each command with each command set (4 cases).
 * 4. Concatenate the sets and execute both commands with the combined set.
 * 5. Clean up all sets and commands.
 */
#include <stdio.h>
#include <string.h>
#include "esp_cli_commands.h"
#include "command_utils.h"

static int cmd_a_handler(void *context, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)context; (void)argc; (void)argv;
    const char *msg = "cmd_a executed\n";
    const size_t msg_len = sizeof(msg) - 1;
    const int nwrite = cmd_arg->write_func(cmd_arg->out_fd, msg, msg_len);
    return nwrite == msg_len ? 0 : -1;
}

static int cmd_b_handler(void *context, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)context; (void)argc; (void)argv;
    const char *msg = "cmd_b executed\n";
    const size_t msg_len = sizeof(msg) - 1;
    const int nwrite = cmd_arg->write_func(cmd_arg->out_fd, msg, msg_len);
    return nwrite == msg_len ? 0 : -1;
}

void app_main(void)
{
    printf("esp_cli_commands command_set example started.\n");

    esp_cli_commands_exec_arg_t cmd_args = {
        .out_fd = STDOUT_FILENO,
        .write_func = write,
        .dynamic_ctx = NULL
    };

    // Define two commands
    esp_cli_command_t cmd_a = {
        .name = "cmd_a",
        .group = "group_a",
        .help = "Command A",
        .func = cmd_a_handler,
        .func_ctx = NULL,
        .hint_cb = NULL,
        .glossary_cb = NULL
    };
    esp_cli_command_t cmd_b = {
        .name = "cmd_b",
        .group = "group_b",
        .help = "Command B",
        .func = cmd_b_handler,
        .func_ctx = NULL,
        .hint_cb = NULL,
        .glossary_cb = NULL
    };

    ESP_ERROR_CHECK(esp_cli_commands_register_cmd(&cmd_a));
    ESP_ERROR_CHECK(esp_cli_commands_register_cmd(&cmd_b));

    // create command sets. One with command name, another with group name
    const char *cmd_set_a[] = { "cmd_a" };
    const char *cmd_set_b[] = { "group_b" };
    esp_cli_command_set_handle_t set_a = ESP_CLI_COMMANDS_CREATE_CMD_SET(cmd_set_a, ESP_CLI_COMMAND_FIELD_ACCESSOR(name));
    esp_cli_command_set_handle_t set_b = ESP_CLI_COMMANDS_CREATE_CMD_SET(cmd_set_b, ESP_CLI_COMMAND_FIELD_ACCESSOR(group));

    // Test all combinations
    int ret = -1;
    printf("-- Executing cmd_a with set_a (should succeed) --\n");
    ESP_ERROR_CHECK(esp_cli_commands_execute("cmd_a", &ret, set_a, &cmd_args));

    printf("-- Executing cmd_b with set_b (should succeed) --\n");
    ESP_ERROR_CHECK(esp_cli_commands_execute("cmd_b", &ret, set_b, &cmd_args));

    printf("-- Executing cmd_a with set_b (should fail) --\n");
    esp_err_t ret_val = esp_cli_commands_execute("cmd_a", &ret, set_b, &cmd_args);
    if (ret_val != ESP_OK) {
        printf("Expected failure: cmd_a not in set_b\n");
    }

    printf("-- Executing cmd_b with set_a (should fail) --\n");
    ret_val = esp_cli_commands_execute("cmd_b", &ret, set_a, &cmd_args);
    if (ret_val != ESP_OK) {
        printf("Expected failure: cmd_b not in set_a\n");
    }

    // Concatenate sets
    esp_cli_command_set_handle_t set_concat = esp_cli_commands_concat_cmd_set(set_a, set_b);
    printf("-- Executing cmd_a with concatenated set (should succeed) --\n");
    ESP_ERROR_CHECK(esp_cli_commands_execute("cmd_a", &ret, set_concat, &cmd_args));
    printf("-- Executing cmd_b with concatenated set (should succeed) --\n");
    ESP_ERROR_CHECK(esp_cli_commands_execute("cmd_b", &ret, set_concat, &cmd_args));

    // Cleanup
    esp_cli_commands_destroy_cmd_set(&set_concat);
    ESP_ERROR_CHECK(esp_cli_commands_unregister_cmd("cmd_a"));
    ESP_ERROR_CHECK(esp_cli_commands_unregister_cmd("cmd_b"));

    printf("end of example\n");
}