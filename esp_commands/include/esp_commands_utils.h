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
#include <stdint.h>

#define _ESP_REPL_STRINGIFY(x) #x
#define ESP_REPL_STRINGIFY(x) _ESP_REPL_STRINGIFY(x)

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
 * @brief Structure containing dynamic argument necessary for the
 * command callback to execute properly.
 *
 * @note Since a command function callback can be executed from
 * a random context, the callback has to be aware of what file descriptor
 * and what write function to use in order to print data to the expected
 * destination.
 */
typedef struct esp_commands_exec_arg {
    int out_fd; /*!< file descriptor that the command function has to use to print data in the environment it was called from */
    esp_commands_write_t write_func; /*!< write function the command function has to use to print data in the environment it was called from */
    void *dynamic_ctx; /*!< dynamic context passed to the command function */
} esp_commands_exec_arg_t;

/**
 * @brief Console command main function type with user context
 *
 * This function type is used to implement a console command.
 *
 * @param context User-defined context passed at invocation
 * @param cmd_arg Structure containing dynamic arguments necessary for the command
 * @param argc Number of arguments
 * @param argv Array of argc entries, each pointing to a null-terminated string argument
 * @return Return code of the console command; 0 indicates success
 */
typedef int (*esp_command_func_t)(void *context, esp_commands_exec_arg_t *cmd_arg, int argc, char **argv);

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
 * @brief Configuration parameters for esp_commands_manager initialization
 */
typedef struct esp_commands_config {
    uint32_t heap_caps_used;            /*!< Set of heap capabilities to be used to perform internal allocations */
    size_t max_cmdline_length;          /*!< Maximum length of the command line buffer, in bytes */
    size_t max_cmdline_args;            /*!< Maximum number of command line arguments to parse */
    int hint_color;                     /*!< ANSI color code used for hint text */
    bool hint_bold;                     /*!< If true, display hint text in bold */
} esp_commands_config_t;

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
 * @note Those accessor functions are defined in esp_commands_internal.h
 *
 * @param NAME Field name of esp_command_t
 */
#define FIELD_ACCESSOR(NAME) get_##NAME

#ifdef __cplusplus
}
#endif
