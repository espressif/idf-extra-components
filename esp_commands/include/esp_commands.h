/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_heap_caps.h"

/**
 * @brief Console command main function
 * @param argc number of arguments
 * @param argv array with argc entries, each pointing to a zero-terminated string argument
 * @return console command return code, 0 indicates "success"
 */
typedef int (*esp_command_func_t)(int argc, char **argv);

/**
 * @brief Console command main function, with context
 * @param context a user context given at invocation
 * @param argc number of arguments
 * @param argv array with argc entries, each pointing to a zero-terminated string argument
 * @return console command return code, 0 indicates "success"
 */
typedef int (*esp_command_func_w_ctx_t)(void *context, int argc, char **argv);

/**
 * @brief Console command get hint function callback
 *
 * @return Generated hint string for the given command
 */
typedef const char *(*esp_command_hint_t)(void);

/**
 * @brief Console command get glossary function callback
 *
 * @return Generated glossary string for the given command
 */
typedef const char *(*esp_command_glossary_t)(void);

/**
 * @brief Console command description
 *
 * @note func and func_w_ctx cannot be both set. One of them must be set.
 * @note the group field will assign the command to a given group based on the
 * name this field will be given. This allows the user to generate a command
 * set based on group name rather than command name if needed. It can prove useful
 * when the number of commands that should compose a set is big.
 */
typedef struct esp_command {
    const char *name; /*!< Name of the command */
    const char *group; /*!< Command group of which the command belongs */
    const char *help; /*!< Help of the command */
    esp_command_func_t func; /*!< Pointer to a function which implements the command */
    esp_command_func_w_ctx_t func_w_ctx; /*!< Pointer to a context aware function which implements the command */
    void *func_ctx; /*!< Context pointer to user-defined per-command context data */
    esp_command_hint_t hint_cb; /*!< Pointer to a function returning the hint of the command*/
    esp_command_glossary_t glossary_cb; /*!< Pointer to a function returning the glossary of the command*/
} esp_command_t;

/**
 * @brief macro registering a command and placing it in a specific section of flash.rodata
 * @note see the linker.lf file for more information concerning the section characteristics
 */
#define ESP_COMMAND_REGISTER(cmd_name, cmd_group, cmd_help, cmd_func, cmd_func_w_ctx, cmd_func_ctx, cmd_hint_cb, cmd_glossary_cb) \
    static_assert(((cmd_func) == NULL) != ((cmd_func_w_ctx) == NULL)); \
    static const esp_command_t cmd_name __attribute__((used, section(".esp_commands"))) = { \
        .name = #cmd_name, \
        .group = #cmd_group, \
        .help = cmd_help, \
        .func = cmd_func, \
        .func_w_ctx = cmd_func_w_ctx, \
        .func_ctx = cmd_func_ctx, \
        .hint_cb = cmd_hint_cb, \
        .glossary_cb = cmd_glossary_cb \
    };

/**
 * @brief create a forced inline function returning a given
 * esp_command_t field
 */
#define DEFINE_FIELD_ACCESSOR(NAME) \
    static inline __attribute__((always_inline)) \
    const char *get_##NAME(const esp_command_t *cmd) { \
        return cmd->NAME; \
    }

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_name(esp_command_t *cmd) {
 *     return cmd->name;
 * }
 */
DEFINE_FIELD_ACCESSOR(name)

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_group(esp_command_t *cmd) {
 *     return cmd->group;
 * }
 */
DEFINE_FIELD_ACCESSOR(group)

/**
 * @brief Macro expanding to
 * static inline __attribute__((always_inline)) const char *get_help(esp_command_t *cmd) {
 *     return cmd->help;
 * }
 */
DEFINE_FIELD_ACCESSOR(help)

/**
 * @brief Create a function name based on the NAME token
 * @note NAME must match a field from esp_command_t returning
 * a string.
 */
#define FIELD_ACCESSOR(NAME) get_##NAME

/**
 * @brief Configuration parameters for esp_commands_manager initialization
 */
typedef struct esp_commands_config {
    size_t max_cmdline_length;  /*!< length of command line buffer, in bytes */
    size_t max_cmdline_args;    /*!< maximum number of command line arguments to parse */
    uint32_t heap_alloc_caps;   /*!< where to (e.g. MALLOC_CAP_SPIRAM) allocate heap objects such as cmds used by esp_console */
    int hint_color;             /*!< ASCII color code of hint text */
    int hint_bold;              /*!< Set to 1 to print hint text in bold */
} esp_commands_config_t;

/**
 * @brief Default esp_commands_manager configuration value
 */
#define ESP_COMMANDS_CONFIG_DEFAULT() \
{ \
    .max_cmdline_length = 256, \
    .max_cmdline_args = 32, \
    .heap_alloc_caps = MALLOC_CAP_DEFAULT, \
    .hint_color = 39, \
    .hint_bold = 0 \
}

/**
 * @brief Called from esp_commands_get_completion with a complete
 * valid command name
 *
 * @param completed_cmd_name The completed command name
 */
typedef void (*esp_command_get_completion_t)(const char *completed_cmd_name);

/**
 * @brief Called to retrieve the value of a specific esp_command_t field
 */
typedef const char *(*esp_commands_get_field_t)(const esp_command_t *cmd);

/**
 * @brief Handle to a set of commands
 */
typedef struct esp_command_set *esp_command_set_handle_t;

/**
 * @brief
 *
 * @param config
 * @return esp_err_t
 */
esp_err_t esp_commands_init(const esp_commands_config_t *config);

/**
 * @brief
 *
 * @return esp_err_t
 */
esp_err_t esp_commands_deinit(void);

/**
 * @brief Run the command
 *
 * @param cmd_set set of commands allowed to be ran. If the command from the cmd_line if
 * not in the set, it will not be executed and will yield a ESP_ERR_NOT_FOUND return value.
 * @param cmd_line the string containing the command to execute
 * @param cmd_ret the return value from the command function
 * @return esp_err_t
 *      - ESP_OK, if command was run
 *      - ESP_ERR_INVALID_ARG, if the command line is empty, or only contained
 *        whitespace
 *      - ESP_ERR_NOT_FOUND, if command with given name is not found in the command set
 *        passed as parameter
 *      - ESP_ERR_INVALID_STATE, if esp_commands_init wasn't called
 */
esp_err_t esp_commands_execute(esp_command_set_handle_t cmd_set, const char *cmdline, int *cmd_ret);

/**
 * @brief Takes a set of command names and create a set of pointers to the associated
 * command names. Returns a handle to this created set.
 *
 * @note Since
 *
 * @param cmd_set the set of command names that are allowed to be executed on call to
 * esp_commands_execute
 * @return esp_command_set_handle_t handle to the generated set containing pointers
 * to commands associated with the set of names passed as parameter.
 */
esp_command_set_handle_t esp_commands_create_cmd_set(const char **cmd_name_set, const size_t cmd_name_set_size, esp_commands_get_field_t get_field);

/**
 * @brief Create a new command set by concatenating two command sets given as parameter
 *
 * @note if one of the set is NULL, the function will return a handle to the other set.
 * @note If both of the sets are NULL, the function returns with a NULL handle
 * @note The function will not parse the items of each sets. If duplicates are found
 * in the sets, they will be kept in the concatenated command set returned by the function.
 *
 * @param cmd_set_a first command set
 * @param cmd_set_b second command set
 * @return esp_command_set_handle_t command set resulting from the concatenation of
 * the command sets passed as parameter
 */
esp_command_set_handle_t esp_commands_concat_cmd_set(esp_command_set_handle_t cmd_set_a, esp_command_set_handle_t cmd_set_b);

/**
 * @brief Destroys a command set returned by the call of esp_commands_create_cmd_set
 *
 * @param cmd_set The handle to the command set to destroy
 */
void esp_commands_destroy_cmd_set(esp_command_set_handle_t cmd_set);

/**
 * @brief Callback which provides command completion for linenoise library
 *
 * When using linenoise for line editing, command completion support
 * can be enabled like this:
 *
 *   linenoiseSetCompletionCallback(&esp_commands_get_completion);
 *
 * @param buf the string typed by the user
 * @param completion_cb function to call with the completed command name
 */
void esp_commands_get_completion(const char *buf, esp_command_get_completion_t completion_cb);

/**
 * @brief Callback which provides command hints for linenoise library
 *
 * @param buf line typed by the user
 * @param[out] color ANSI color code to be used when displaying the hint
 * @param[out] bold set to 1 if hint has to be displayed in bold
 * @return string containing the hint text. This string is persistent and should
 *         not be freed.
 */
const char *esp_commands_get_hint(const char *buf, int *color, int *bold);

#ifdef __cplusplus
}
#endif
