/* linenoise.h -- VERSION 1.0
 *
 * Guerrilla line editing library against the idea that a line editing lib
 * needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * IMPORTANT: This library is not thread safe.
 */

/**
 * @brief Stores a list of completion strings.
 */
typedef struct esp_linenoise_completions {
    size_t len;   /**< Number of completions. */
    char **cvec;  /**< Array of completion strings. */
} esp_linenoise_completions_t;

/**
 * @brief Callback function for providing completions.
 *
 * @param str Input string from the user.
 * @param lc Pointer to the completions object to be populated.
 * @param user_ctx User-defined context pointer passed from the configuration.
 */
typedef void (*esp_linenoise_completion_callback)(const char *str, esp_linenoise_completions_t *lc, void *user_ctx);

/**
 * @brief Callback function for providing inline hints during input.
 *
 * @param str Input string from the user.
 * @param color Pointer to an integer to set the hint text color (e.g., ANSI color code).
 * @param bold Pointer to an integer to set bold attribute (non-zero for bold).
 * @param user_ctx User-defined context pointer passed from the configuration.
 * @return A dynamically allocated hint string, or NULL if no hint is available.
 */
typedef char *(*esp_linenoise_hints_callback)(const char *str, int *color, int *bold, void *user_ctx);

/**
 * @brief Callback function to free hint strings returned by the hints callback.
 *
 * @param ptr Pointer to the hint string to be freed.
 * @param user_ctx User-defined context pointer passed from the configuration.
 */
typedef void (*esp_linenoise_free_hints_callback)(void *ptr, void *user_ctx);

/**
 * @brief Function pointer type for reading bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer to store the read bytes.
 * @param count Number of bytes to read.
 * @param user_ctx User-defined context pointer passed from the configuration.
 * @return Number of bytes read, or -1 on error.
 */
typedef ssize_t (*esp_linenoise_read_bytes_fn)(int fd, void *buf, size_t count, void *user_cx);

/**
 * @brief Function pointer type for writing bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer containing bytes to write.
 * @param count Number of bytes to write.
 * @param user_ctx User-defined context pointer passed from the configuration.
 * @return Number of bytes written, or -1 on error.
 */
typedef ssize_t (*esp_linenoise_write_bytes_fn)(int fd, const void *buf, size_t count, void *user_ctx);

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
    esp_linenoise_completion_callback completion_cb; /*!< Callback function for handling input completions */
    esp_linenoise_hints_callback hints_cb; /*!< Callback function to provide input hints (e.g., usage suggestions) */
    esp_linenoise_free_hints_callback free_hints_cb; /*!< Callback function to free hints returned by `hints_cb` */
    void *user_ctx; /*!< User context information passed to the callbacks */
    esp_linenoise_read_bytes_fn read_bytes_fn; /*!< Function used to read bytes from the input stream */
    esp_linenoise_write_bytes_fn write_bytes_fn; /*!< Function used to write bytes to the output stream */
    char **history; /*!< Pointer to the history buffer (used internally; typically initialized to NULL) */
} esp_linenoise_config_t;

/**
 * @brief Opaque handle to a linenoise instance.
 */
typedef struct esp_linenoise_instance *esp_linenoise_handle_t;

/**
 * @brief Returns the default parameters for creating a linenoise instance.
 *
 * @param config esp_linenoise_config_t structure to populate with default values.
 */
void esp_linenoise_get_instance_config_default(esp_linenoise_config_t *config);

/**
 * @brief Creates a new linenoise instance.
 *
 * @param param Pointer to the configuration parameters for the instance.
 * @return Handle to the created instance, or NULL on failure.
 */
esp_linenoise_handle_t esp_linenoise_create_instance(esp_linenoise_config_t *param);

/**
 * @brief Destroys a linenoise instance and frees associated memory.
 *
 * @param handle Handle to the instance to delete.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_delete_instance(esp_linenoise_handle_t handle);

/**
 * @brief This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example.
 *
 * @param lc line completion structure being filled by the function
 * @param str completed command to add to lc
 */
void esp_linenoise_add_completion(esp_linenoise_completions_t *lc, const char *str);

/**
 * @brief Reads a line of input from the user into a buffer.
 *
 * @param handle Handle to the linenoise instance.
 * @param cmd_line_buffer Buffer to store the input line.
 * @param cmd_line_length Length of the command line buffer.
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t esp_linenoise_get_line(esp_linenoise_handle_t handle, char *cmd_line_buffer, size_t cmd_line_length);

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
