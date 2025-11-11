/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_cli_commands_utils.h"
#include "esp_heap_caps.h"
#include "esp_err.h"

/**
 * @brief Update the component configuration
 *
 * @param config Configuration data to update
 * @return ESP_OK if successful
 *         ESP_ERR_INVALID_ARG if config pointer is NULL
 */
esp_err_t esp_cli_commands_update_config(const esp_cli_commands_config_t *config);

/**
 * @brief macro registering a command and placing it in a specific section of flash.rodata
 * @note see the linker.lf file for more information concerning the section characteristics
 */
#define _ESP_CLI_COMMAND_REGISTER(cmd_name, cmd_group, cmd_help, cmd_func, cmd_func_ctx, cmd_hint_cb, cmd_glossary_cb) \
    static_assert((cmd_func) != NULL); \
    static const esp_cli_command_t cmd_name __attribute__((used, section(".esp_cli_commands" "." _ESP_REPL_STRINGIFY(cmd_name)), aligned(4))) = { \
        .name = _ESP_REPL_STRINGIFY(cmd_name), \
        .group = _ESP_REPL_STRINGIFY(cmd_group), \
        .help = cmd_help, \
        .func = cmd_func, \
        .func_ctx = cmd_func_ctx, \
        .hint_cb = cmd_hint_cb, \
        .glossary_cb = cmd_glossary_cb \
    };

#define ESP_CLI_COMMAND_REGISTER(cmd_name, cmd_group, cmd_help, cmd_func, cmd_func_ctx, cmd_hint_cb, cmd_glossary_cb) \
    _ESP_CLI_COMMAND_REGISTER(cmd_name, cmd_group, cmd_help, cmd_func, cmd_func_ctx, cmd_hint_cb, cmd_glossary_cb)

/**
 * @brief Register a command
 *
 * @param cmd Pointer to the command structure
 * @return ESP_OK if successful
 *         Other esp_err_t on error
 */
esp_err_t esp_cli_commands_register_cmd(esp_cli_command_t *cmd);

/**
 * @brief Unregister a command by name or group
 *
 * @param cmd_name Name or group of the command to unregister
 * @return ESP_OK if successful
 *         Other esp_err_t on error
 */
esp_err_t esp_cli_commands_unregister_cmd(const char *cmd_name);

/**
 * @brief Execute a command line
 *
 * @param cmd_line Command line string to execute
 * @param cmd_ret Return value from the command function. If -1, standard output will be used.
 * @param cmd_set Set of commands allowed to execute. If NULL, all registered commands are allowed
 * @param cmd_arg Structure containing dynamic arguments necessary for the command
 * callback to execute properly
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if the command line is empty or only whitespace
 *         ESP_ERR_NOT_FOUND if command is not found in cmd_set
 *         ESP_ERR_NO_MEM if internal memory allocation fails
 */
esp_err_t esp_cli_commands_execute(const char *cmdline, int *cmd_ret, esp_cli_command_set_handle_t cmd_set, esp_cli_commands_exec_arg_t *cmd_args);

/**
 * @brief Find a command by name within a specific command set.
 *
 * This function searches a command whose name matches the provided string.
 *
 * @param cmd_set Handle to the command set to search in. Must be a valid
 * `esp_cli_command_set_handle_t` or `NULL` if the search should be performed
 * on all statically and dynamically registered commands.
 * @param name String containing the name of the command to search for.
 *
 * @return pointer to the matching command or NULL if no command is found.
 */
esp_cli_command_t *esp_cli_commands_find_command(esp_cli_command_set_handle_t cmd_set, const char *name);

/**
 * @brief Provide command completion for linenoise library
 *
 * @param cmd_set Set of commands allowed for completion. If NULL, all registered commands are used
 * @param buf Input string typed by the user
 * @param cb_ctx context passed to the completion callback
 * @param completion_cb Callback to return completed command names
 */
void esp_cli_commands_get_completion(esp_cli_command_set_handle_t cmd_set, const char *buf, void *cb_ctx, esp_cli_command_get_completion_t completion_cb);

/**
 * @brief Provide command hint for linenoise library
 *
 * @param cmd_set Set of commands allowed for hinting. If NULL, all registered commands are used
 * @param buf Input string typed by the user
 * @param[out] color ANSI color code for hint text
 * @param[out] bold True if hint should be displayed in bold
 * @return Persistent string containing the hint; must not be freed
 */
const char *esp_cli_commands_get_hint(esp_cli_command_set_handle_t cmd_set, const char *buf, int *color, bool *bold);

/**
 * @brief Retrieve glossary for a command line
 *
 * @param cmd_set Set of commands allowed
 * @param buf Command line typed by the user
 * @return Persistent string containing the glossary; must not be freed
 */
const char *esp_cli_commands_get_glossary(esp_cli_command_set_handle_t cmd_set, const char *buf);

/**
 * @brief Create a command set from an array of command names
 *
 * @param cmd_set Array of command names
 * @param cmd_set_size Number of entries in cmd_set
 * @param get_field Function to retrieve the field from esp_cli_command_t for comparison
 * @return Handle to the created command set
 */
esp_cli_command_set_handle_t esp_cli_commands_create_cmd_set(const char **cmd_set, const size_t cmd_set_size, esp_cli_commands_get_field_t get_field);

/**
 * @brief Convenience macro to create a command set
 *
 * @param cmd_set Array of command names
 * @param accessor Field accessor function
 */
#define ESP_CLI_COMMANDS_CREATE_CMD_SET(cmd_set, accessor) \
    esp_cli_commands_create_cmd_set(cmd_set, sizeof(cmd_set) / sizeof((cmd_set)[0]), accessor)

/**
 * @brief Concatenate two command sets
 *
 * @note If one set is NULL, the other is returned
 * @note If both are NULL, returns NULL
 * @note Duplicates are not removed
 *
 * @param cmd_set_a First command set
 * @param cmd_set_b Second command set
 * @return New command set containing all commands from both sets
 */
esp_cli_command_set_handle_t esp_cli_commands_concat_cmd_set(esp_cli_command_set_handle_t cmd_set_a, esp_cli_command_set_handle_t cmd_set_b);

/**
 * @brief Destroy a command set
 *
 * @param cmd_set Pointer to the handle of the command set to destroy
 */
void esp_cli_commands_destroy_cmd_set(esp_cli_command_set_handle_t *cmd_set);

/**
 * @brief Split a command line and populate argc and argv parameters
 *
 * @param line the line that has to be split into arguments
 * @param argv array of arguments created from the line
 * @param argv_size size of the argument array
 * @return size_t number of arguments found in the line and stored
 * in argv
 */
size_t esp_cli_commands_split_argv(char *line, char **argv, size_t argv_size);

#ifdef __cplusplus
}
#endif
