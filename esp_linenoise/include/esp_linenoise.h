/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * IMPORTANT: This library is not thread safe.
 * Multiple threads operating on one esp_linenoise instance
 * can yield unexpected behavior from the esp_linenoise library.
 *
 * An esp_linenoise instance is to be used by one thread only.
 */

/**
 * @brief Callback type used to report a completion candidate to esp_linenoise
 *
 * This callback is invoked by `esp_linenoise_completion_t` for each possible
 * completion string.
 *
 * @note When esp_linenoise calls `esp_linenoise_completion_t`, it provides
 * `esp_linenoise_add_completion` as the `esp_linenoise_completion_cb_t`.
 * This allows user code to return completion candidates back into
 * esp_linenoise.
 *
 * @note esp_linenoise also passes an opaque pointer of type
 * `esp_linenoise_completions_t` as the `cb_ctx`. User code must forward
 * this `cb_ctx` unchanged when invoking the callback. This design hides
 * the internal structure of esp_linenoise from the user.
 *
 * @param cb_ctx Opaque context pointer provided by esp_linenoise. Must be
 *               passed unchanged when calling the callback.
 * @param str A candidate completion string.
 */
typedef void (*esp_linenoise_completion_cb_t)(void *cb_ctx, const char *str);

/**
 * @brief User-provided callback type for generating command completions.
 *
 * This function is called by esp_linenoise when the user requests tab
 * completion. The implementation should analyze the input string and invoke
 * the provided completion callback (`cb`) for each possible completion.
 *
 * configuration. Allows the completion function to access application-specific state.
 * @param str The current input string typed by the user.
 * @param cb_ctx Opaque pointer provided by esp_linenoise. This must be
 * forwarded unchanged when calling the completion callback.
 * @param cb Callback function to be invoked once for each candidate
 * completion string discovered by this function.
 */
typedef void (*esp_linenoise_completion_t)(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb);

/**
 * @brief Callback function for providing inline hints during input.
 *
 * @param str Input string from the user.
 * @param color Pointer to an integer to set the hint text color (e.g., ANSI color code).
 * @param bold Pointer to an integer to set bold attribute (non-zero for bold).
 * @return A dynamically allocated hint string, or NULL if no hint is available.
 */
typedef char *(*esp_linenoise_hints_t)(const char *str, int *color, int *bold);

/**
 * @brief Callback function to free hint strings returned by the hints callback.
 *
 * @param ptr Pointer to the hint string to be freed.
 */
typedef void (*esp_linenoise_free_hints_t)( void *ptr);

/**
 * @brief Function pointer type for reading bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer to store the read bytes.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or -1 on error.
 */
typedef ssize_t (*esp_linenoise_read_bytes_t)(int fd, void *buf, size_t count);

/**
 * @brief Function pointer type for writing bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer containing bytes to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or -1 on error.
 */
typedef ssize_t (*esp_linenoise_write_bytes_t)(int fd, const void *buf, size_t count);

/**
 * @brief Structure defining the parameters needed by a linenoise
 * instance
 */
typedef struct esp_linenoise_config {
    const char *prompt; /*!< Prompt string displayed to the user */
    size_t max_cmd_line_length; /*!< Maximum length (in bytes) of the input command line */
    int history_max_length; /*!< Maximum number of entries to store in command history */
    int in_fd; /*!< File descriptor to read input from (e.g., STDIN_FILENO) */
    int out_fd; /*!< File descriptor to write output to (e.g., STDOUT_FILENO) */
    bool allow_multi_line; /*!< Whether to allow multi-line input (true to enable) */
    bool allow_empty_line; /*!< Whether to allow accepting an empty line as valid input */
    bool allow_dumb_mode; /*!< Whether to allow running in dumb terminal mode (without advanced features) */
    esp_linenoise_completion_t completion_cb; /*!< Callback function for handling input completions */
    esp_linenoise_hints_t hints_cb; /*!< Callback function to provide input hints (e.g., usage suggestions) */
    esp_linenoise_free_hints_t free_hints_cb; /*!< Callback function to free hints returned by `hints_cb` */
    esp_linenoise_read_bytes_t read_bytes_cb; /*!< Function used to read bytes from the input stream */
    esp_linenoise_write_bytes_t write_bytes_cb; /*!< Function used to write bytes to the output stream */
    char **history; /*!< Pointer to the history buffer (used internally; typically initialized to NULL) */
} esp_linenoise_config_t;

/**
 * @brief Opaque handle to a linenoise instance.
 */
typedef struct esp_linenoise_instance *esp_linenoise_handle_t;

/**
 * @brief Probe the terminal to check weather it supports escape sequences
 *
 * @param handle The linenoise handle used to check
 * @return int 0 if the terminal supports escape sequences
 */
int esp_linenoise_probe(esp_linenoise_handle_t handle);

/**
 * @brief Returns the default parameters for creating a linenoise instance.
 *
 * @param[out] config esp_linenoise_config_t structure to populate with default values.
 */
void esp_linenoise_get_instance_config_default(esp_linenoise_config_t *config);

/**
 * @brief Creates a new linenoise instance.
 *
 * @param config Pointer to the configuration parameters for the instance.
 * @param[out] out_handle Handle to the created instance, or NULL on failure.
 * @return ESP_OK if instance created successfully,
 */
esp_err_t esp_linenoise_create_instance(const esp_linenoise_config_t *config, esp_linenoise_handle_t *out_handle);

/**
 * @brief Destroys a linenoise instance and frees associated memory.
 *
 * @param handle Handle to the instance to delete.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_delete_instance(esp_linenoise_handle_t handle);

/**
 * @brief Reads a line of input from the user into a buffer.
 *
 * @note In the event where the input line matches or exceeds
 * the size of the buffer passed in parameter, the function
 * will stop registering newly received characters until new line
 * is received. Then, the function will return cmd_line_length - 1
 * characters (the last character being the nullterm '\0')
 * This behavior applies whether the dumb mode is on or off.
 *
 * @param handle Handle to the linenoise instance.
 * @param cmd_line_buffer Buffer to store the input line.
 * @param cmd_line_length Length of the command line buffer.
 * @return ESP_OK on success.
 *         ESP_FAIL if empty line returned and allow_empty_line is set to false.
 *         ESP_ERR_INVALID_ARG if cmd_line_buffer is NULL or cmd_line_length
 *         is equal to 0 or superior to the value of max_cmd_line_length.
 */
esp_err_t esp_linenoise_get_line(esp_linenoise_handle_t handle, char *cmd_line_buffer, size_t cmd_line_length);

/**
 * @brief Triggers an internal mechanism to return from esp_linenoise_get_line.
 *
 * @note This function has no effect if the field read_bytes_cb of
 * esp_linenoise_config_t is populate with a custom read function.
 * In that case, it is the user responsibility to handle the way to
 * return from the custom read it provided to linenoise.
 *
 * @param handle Handle to the linenoise instance.
 * @return esp_err_t ESP_OK on success, other on failures.
 */
esp_err_t esp_linenoise_abort(esp_linenoise_handle_t handle);

/**
 * @brief Adds a line to the instance's history.
 *
 * @param handle Handle to the linenoise instance.
 * @param line The line to add to history.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_history_add(esp_linenoise_handle_t handle, const char *line);

/**
 * @brief Saves the history to a file.
 *
 * @param handle Handle to the linenoise instance.
 * @param filename Path to the history file.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_history_save(esp_linenoise_handle_t handle, const char *filename);

/**
 * @brief Loads history from a file.
 *
 * @param handle Handle to the linenoise instance.
 * @param filename Path to the history file.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_history_load(esp_linenoise_handle_t handle, const char *filename);

/**
 * @brief Sets the maximum number of entries in the history.
 *
 * @param handle Handle to the linenoise instance.
 * @param len Maximum number of entries.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_history_set_max_len(esp_linenoise_handle_t handle, int len);

/**
 * @brief Frees the history associated with the instance.
 *
 * @param handle Handle to the linenoise instance.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_history_free(esp_linenoise_handle_t handle);

/**
 * @brief Clears the terminal screen for the instance.
 *
 * @param handle Handle to the linenoise instance.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_clear_screen(esp_linenoise_handle_t handle);

/**
 * @brief Sets whether an empty line is allowed to be returned.
 *
 * @param handle Handle to the linenoise instance.
 * @param empty_line true to allow empty lines, false to disallow.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_set_empty_line(esp_linenoise_handle_t handle, bool empty_line);

/**
 * @brief Checks whether the instance allows returning an empty line.
 *
 * @param handle Handle to the linenoise instance.
 * @param is_empty_line Pointer to bool that will be set with result.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_is_empty_line(esp_linenoise_handle_t handle, bool *is_empty_line);

/**
 * @brief Enables or disables multi-line editing.
 *
 * @param handle Handle to the linenoise instance.
 * @param multi_line true to enable multi-line input, false to disable.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_set_multi_line(esp_linenoise_handle_t handle, bool multi_line);

/**
 * @brief Checks if multi-line editing is enabled.
 *
 * @param handle Handle to the linenoise instance.
 * @param is_multi_line Pointer to bool that will be set with result.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_is_multi_line(esp_linenoise_handle_t handle, bool *is_multi_line);

/**
 * @brief Enables or disables dumb mode (disables line editing features).
 *
 * @param handle Handle to the linenoise instance.
 * @param dumb_mode true to enable dumb mode, false to disable.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_set_dumb_mode(esp_linenoise_handle_t handle, bool dumb_mode);

/**
 * @brief Checks if dumb mode is enabled.
 *
 * @param handle Handle to the linenoise instance.
 * @param is_dumb_mode Pointer to bool that will be set with result.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_is_dumb_mode(esp_linenoise_handle_t handle, bool *is_dumb_mode);

/**
 * @brief Sets the maximum length of the command line buffer.
 *
 * @param handle Handle to the linenoise instance.
 * @param length Maximum length in bytes.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_set_max_cmd_line_length(esp_linenoise_handle_t handle, size_t length);

/**
 * @brief Gets the maximum allowed command line buffer length.
 *
 * @param handle Handle to the linenoise instance.
 * @param max_cmd_line_length Pointer to receive the max length.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_get_max_cmd_line_length(esp_linenoise_handle_t handle, size_t *max_cmd_line_length);

/**
 * @brief Sets the prompt used by the esp_linenoise instance.
 *
 * @param handle Handle to the linenoise instance.
 * @param prompt Prompt to be set.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_set_prompt(esp_linenoise_handle_t handle, const char *prompt);

/**
 * @brief Gets the current esp_linenoise instance prompt.
 *
 * @param handle Handle to the linenoise instance.
 * @param prompt esp_linenoise instance current prompt.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_get_prompt(esp_linenoise_handle_t handle, const char **prompt);

/**
 * @brief Return the output file descriptor used by esp_linenoise
 *
 * @param handle The esp_linenoise handle from which to get
 * the file descriptor
 * @param fd Return value containing the output file descriptor
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t esp_linenoise_get_out_fd(esp_linenoise_handle_t handle, int *fd);

/**
 * @brief Return the input file descriptor used by esp_linenoise
 *
 * @param handle The esp_linenoise handle from which to get
 * the file descriptor
 * @param fd Return value containing the input file descriptor
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t esp_linenoise_get_in_fd(esp_linenoise_handle_t handle, int *fd);

/**
 * @brief Return the read function used by linenoise
 *
 * @param handle The esp_linenoise handle from which to get
 * the file descriptor
 * @param read_func Return the read_func as set in the configuration structure
 * of the given esp_linenoise instance
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t esp_linenoise_get_read(esp_linenoise_handle_t handle, esp_linenoise_read_bytes_t *read_func);

/**
 * @brief Return the write function used by linenoise
 *
 * @param handle The esp_linenoise handle from which to get
 * the file descriptor
 * @param write_func Return the write_func as set in the configuration structure
 * of the given esp_linenoise instance
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t esp_linenoise_get_write(esp_linenoise_handle_t handle, esp_linenoise_write_bytes_t *write_func);

#ifdef __cplusplus
}
#endif
