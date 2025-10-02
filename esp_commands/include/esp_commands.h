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
#include "esp_err.h"

/**
 * @brief Console command main function type with user context
 *
 * This function type is used to implement a console command.
 *
 * @param context User-defined context passed at invocation
 * @param fd_out The file descriptor to use to output data
 * @param argc Number of arguments
 * @param argv Array of argc entries, each pointing to a null-terminated string argument
 * @return Return code of the console command; 0 indicates success
 */
typedef int (*esp_command_func_t)(void *context, const int fd_out, int argc, char **argv);

/**
 * @brief Callback to generate a command hint
 *
 * This function is called to retrieve a short hint for a command,
 * typically used for auto-completion or UI help.
 *
 * @param context Context registered when the command was registered
 * @return Persistent string containing the generated hint
 */
typedef const char *(*esp_command_hint_t)(void *context);

/**
 * @brief Callback to generate a command glossary entry
 *
 * This function is called to retrieve detailed description or glossary
 * information for a command.
 *
 * @param context Context registered when the command was registered
 * @return Persistent string containing the generated glossary
 */
typedef const char *(*esp_command_glossary_t)(void *context);

/**
 * @brief Structure describing a console command
 *
 * @note Only one of `func` or `func_ctx` should be set.
 * @note The `group` field allows categorizing commands into groups,
 * which can simplify filtering or listing commands.
 */
typedef struct esp_command {
    const char *name; /*!< Name of the command */
    const char *group; /*!< Command group to which this command belongs */
    const char *help; /*!< Short help text for the command */
    esp_command_func_t func; /*!< Function implementing the command */
    void *func_ctx; /*!< User-defined context for the command function */
    esp_command_hint_t hint_cb; /*!< Callback returning the hint for the command */
    esp_command_glossary_t glossary_cb; /*!< Callback returning the glossary for the command */
} esp_command_t;

/**
 * @brief Macro to define a forced inline accessor for a string field of esp_command_t
 *
 * @param NAME Field name of the esp_command_t structure
 */
#define DEFINE_FIELD_ACCESSOR(NAME) \
    static inline __attribute__((always_inline)) \
    const char *get_##NAME(const esp_command_t *cmd) { \
        if (!cmd) { \
            return NULL; \
        } \
        return cmd->NAME; \
    }

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_name(esp_command_t *cmd) {
 *     if (!cmd) {
 *         return NULL;
 *     }
 *     return cmd->name;
 * }
 */
DEFINE_FIELD_ACCESSOR(name)

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_group(esp_command_t *cmd) {
 *     if (!cmd) {
 *         return NULL;
 *     }
 *     return cmd->group;
 * }
 */
DEFINE_FIELD_ACCESSOR(group)

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_help(esp_command_t *cmd) {
 *     if (!cmd) {
 *         return NULL;
 *     }
 *     return cmd->help;
 * }
 */
DEFINE_FIELD_ACCESSOR(help)

/**
 * @brief Macro to create the accessor function name for a field of esp_command_t
 *
 * @param NAME Field name of esp_command_t
 */
#define FIELD_ACCESSOR(NAME) get_##NAME

/**
 * @brief Function pointer type for writing bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer containing bytes to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or -1 on error.
 */
typedef ssize_t (*esp_commands_write_t)(int fd, const void *buf, size_t count);

/**
 * @brief Configuration parameters for esp_commands_manager initialization
 */
typedef struct esp_commands_config {
    esp_commands_write_t write_func;    /*!< Write function to call when executing a command */
    size_t max_cmdline_length;          /*!< Maximum length of the command line buffer, in bytes */
    size_t max_cmdline_args;            /*!< Maximum number of command line arguments to parse */
    int hint_color;                     /*!< ANSI color code used for hint text */
    bool hint_bold;                     /*!< If true, display hint text in bold */
} esp_commands_config_t;

/**
 * @brief Default configuration for esp_commands_manager
 */
#define ESP_COMMANDS_CONFIG_DEFAULT() \
{ \
    .max_cmdline_length = 256, \
    .max_cmdline_args = 32, \
    .hint_color = 39, \
    .hint_bold = false \
}

/**
 * @brief Callback for a completed command name
 *
 * This callback is called when a command is successfully completed.
 *
 * @param cb_ctx Opaque pointer pointing at the context passed to the callback
 * @param completed_cmd_name Completed command name
 */
typedef void (*esp_command_get_completion_t)(void *cb_ctx, const char *completed_cmd_name);

/**
 * @brief Callback to retrieve a string field of esp_command_t
 *
 * @param cmd Command object
 * @return Value of the requested string field
 */
typedef const char *(*esp_commands_get_field_t)(const esp_command_t *cmd);

/**
 * @brief Opaque handle to a set of commands
 */
typedef struct esp_command_sets *esp_command_set_handle_t;

/**
 * @brief Update the component configuration
 *
 * @param config Configuration data to update
 * @return ESP_OK if successful
 *         ESP_ERR_INVALID_ARG if config pointer is NULL
 */
esp_err_t esp_commands_update_config(const esp_commands_config_t *config);

/**
 * @brief macro registering a command and placing it in a specific section of flash.rodata
 * @note see the linker.lf file for more information concerning the section characteristics
 */
#define ESP_COMMAND_REGISTER(cmd_name, cmd_group, cmd_help, cmd_func, cmd_func_ctx, cmd_hint_cb, cmd_glossary_cb) \
    static_assert((cmd_func) != NULL); \
    static const esp_command_t cmd_name __attribute__((used, section(".esp_commands"))) = { \
        .name = #cmd_name, \
        .group = #cmd_group, \
        .help = cmd_help, \
        .func = cmd_func, \
        .func_ctx = cmd_func_ctx, \
        .hint_cb = cmd_hint_cb, \
        .glossary_cb = cmd_glossary_cb \
    };

/**
 * @brief Register a command
 *
 * @param cmd Pointer to the command structure
 * @return ESP_OK if successful
 *         Other esp_err_t on error
 */
esp_err_t esp_commands_register_cmd(esp_command_t *cmd);

/**
 * @brief Unregister a command by name or group
 *
 * @param cmd_name Name or group of the command to unregister
 * @return ESP_OK if successful
 *         Other esp_err_t on error
 */
esp_err_t esp_commands_unregister_cmd(const char *cmd_name);

/**
 * @brief Execute a command line
 *
 * @param cmd_set Set of commands allowed to execute. If NULL, all registered commands are allowed
 * @param cmd_fd File descriptor used to output data
 * @param cmd_line Command line string to execute
 * @param cmd_ret Return value from the command function. If -1, standard output will be used.
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if the command line is empty or only whitespace
 *         ESP_ERR_NOT_FOUND if command is not found in cmd_set
 *         ESP_ERR_NO_MEM if internal memory allocation fails
 */
esp_err_t esp_commands_execute(esp_command_set_handle_t cmd_set, const int cmd_fd, const char *cmdline, int *cmd_ret);

/**
 * @brief Find a command by name within a specific command set.
 *
 * This function searches a command whose name matches the provided string.
 *
 * @param cmd_set Handle to the command set to search in. Must be a valid
 * `esp_command_set_handle_t` or `NULL` if the search should be performed
 * on all statically and dynamically registered commands.
 * @param name String containing the name of the command to search for.
 *
 * @return pointer to the matching command or NULL if no command is found.
 */
esp_command_t *esp_commands_find_command(esp_command_set_handle_t cmd_set, const char *name);

/**
 * @brief Provide command completion for linenoise library
 *
 * @param cmd_set Set of commands allowed for completion. If NULL, all registered commands are used
 * @param buf Input string typed by the user
 * @param cb_ctx context passed to the completion callback
 * @param completion_cb Callback to return completed command names
 */
void esp_commands_get_completion(esp_command_set_handle_t cmd_set, const char *buf, void *cb_ctx, esp_command_get_completion_t completion_cb);

/**
 * @brief Provide command hint for linenoise library
 *
 * @param cmd_set Set of commands allowed for hinting. If NULL, all registered commands are used
 * @param buf Input string typed by the user
 * @param[out] color ANSI color code for hint text
 * @param[out] bold True if hint should be displayed in bold
 * @return Persistent string containing the hint; must not be freed
 */
const char *esp_commands_get_hint(esp_command_set_handle_t cmd_set, const char *buf, int *color, bool *bold);

/**
 * @brief Retrieve glossary for a command line
 *
 * @param cmd_set Set of commands allowed
 * @param buf Command line typed by the user
 * @return Persistent string containing the glossary; must not be freed
 */
const char *esp_commands_get_glossary(esp_command_set_handle_t cmd_set, const char *buf);

/**
 * @brief Create a command set from an array of command names
 *
 * @param cmd_set Array of command names
 * @param cmd_set_size Number of entries in cmd_set
 * @param get_field Function to retrieve the field from esp_command_t for comparison
 * @return Handle to the created command set
 */
esp_command_set_handle_t esp_commands_create_cmd_set(const char **cmd_set, const size_t cmd_set_size, esp_commands_get_field_t get_field);

/**
 * @brief Convenience macro to create a command set
 *
 * @param cmd_set Array of command names
 * @param accessor Field accessor function
 */
#define ESP_COMMANDS_CREATE_CMD_SET(cmd_set, accessor) \
    esp_commands_create_cmd_set(cmd_set, sizeof(cmd_set) / sizeof((cmd_set)[0]), accessor)

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
esp_command_set_handle_t esp_commands_concat_cmd_set(esp_command_set_handle_t cmd_set_a, esp_command_set_handle_t cmd_set_b);

/**
 * @brief Destroy a command set
 *
 * @param cmd_set Pointer to the handle of the command set to destroy
 */
void esp_commands_destroy_cmd_set(esp_command_set_handle_t *cmd_set);

/**
 * @brief Split a command line and populate argc and argv parameters
 *
 * @param line the line that has to be split into arguments
 * @param argv array of arguments created from the line
 * @param argv_size size of the argument array
 * @return size_t number of arguments found in the line and stored
 * in argv
 */
size_t esp_commands_split_argv(char *line, char **argv, size_t argv_size);

#ifdef __cplusplus
}
#endif
