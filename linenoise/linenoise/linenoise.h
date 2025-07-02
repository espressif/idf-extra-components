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

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * IMPORTANT: This library is not thread safe.
 */

/*------------------------------------------------------------------------------------------------
 *NEW LINENOISE API
 *------------------------------------------------------------------------------------------------*/

/**
 * @brief Stores a list of completion strings.
 */
typedef struct linenoise_completions {
  size_t len;   /**< Number of completions. */
  char **cvec;  /**< Array of completion strings. */
} linenoise_completions_t;

/**
 * @brief Callback function for providing completions.
 *
 * @param str Input string from the user.
 * @param lc Completions object to be populated.
 */
typedef void (*linenoise_completion_callback)(const char *str, linenoise_completions_t *lc);

/**
 * @brief Callback function for providing hints during input.
 *
 * @param str Input string.
 * @param color Pointer to set the color of the hint text.
 * @param bold Pointer to set the bold attribute (non-zero for bold).
 * @return A dynamically allocated hint string or NULL if no hint is available.
 */
typedef char* (*linenoise_hints_callback)(const char *str, int *color, int *bold);

/**
 * @brief Callback function to free memory returned by hints callback.
 *
 * @param ptr Pointer to the hint string to be freed.
 */
typedef void (*linenoise_free_hints_callback)(void *ptr);

/**
 * @brief Function pointer type for reading bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer to store the read bytes.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or -1 on error.
 */
typedef ssize_t (*linenoise_read_bytes_fn)(int fd, void* buf, size_t count);

/**
 * @brief Function pointer type for writing bytes.
 *
 * @param fd File descriptor.
 * @param buf Buffer containing bytes to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or -1 on error.
 */
typedef ssize_t (*linenoise_write_bytes_fn)(int fd, const void* buf, size_t count);

/**
 * @brief Structure defining the parameters needed by a linenoise
 * instance
 */
typedef struct linenoise_instance_param {
    const char *prompt;
    size_t max_cmd_line_length;
    int history_max_length;
    int in_fd;
    int out_fd;
    bool allow_multi_line;
    bool allow_empty_line;
    bool allow_dumb_mode;
    linenoise_completion_callback completion_cb;
    linenoise_hints_callback hints_cb;
    linenoise_free_hints_callback free_hints_cb;
    linenoise_read_bytes_fn read_bytes_fn;
    linenoise_write_bytes_fn write_bytes_fn;
    char **history;
} linenoise_instance_param_t;

/**
 * @brief Opaque handle to a linenoise instance.
 */
typedef struct linenoise_instance *linenoise_handle_t;

/**
 * @brief Returns the default parameters for creating a linenoise instance.
 *
 * @return Default instance parameters.
 */
linenoise_instance_param_t linenoise_get_instance_param_default(void);

/**
 * @brief Creates a new linenoise instance.
 *
 * @param param Configuration parameters for the instance.
 * @return Handle to the created instance, or NULL on failure.
 */
linenoise_handle_t linenoise_create_instance(linenoise_instance_param_t *param);

/**
 * @brief Destroys a linenoise instance and frees associated memory.
 *
 * @param handle Handle to the instance to delete.
 */
void linenoise_delete_instance(linenoise_handle_t handle);

/**
 * @brief Enters the main input loop for the linenoi
 * se instance.
 *
 * @param handle Handle to the linenoise instance.
 * @return A heap-allocated string entered by the user, or NULL on EOF or error.
 *         The returned string must be freed with linenoise_free().
 */
char* linenoise_loop(linenoise_handle_t handle);

/**
 * @brief Frees memory allocated by linenoise (e.g., input strings).
 *
 * @param ptr Pointer to the memory to free.
 */
void linenoise_free(void* ptr);

/**
 * @brief Adds a line to the instance's history.
 *
 * @param handle Handle to the linenoise instance.
 * @param line The line to add to history.
 * @return true on success, false on failure.
 */
bool linenoise_history_add(linenoise_handle_t handle, const char *line);

/**
 * @brief Saves the history to a file.
 *
 * @param handle Handle to the linenoise instance.
 * @param filename Path to the history file.
 * @return true on success, false on failure.
 */
bool linenoise_history_save(linenoise_handle_t handle, const char *filename);

/**
 * @brief Loads history from a file.
 *
 * @param handle Handle to the linenoise instance.
 * @param filename Path to the history file.
 * @return true on success, false on failure.
 */
bool linenoise_history_load(linenoise_handle_t handle, const char *filename);

/**
 * @brief Sets the maximum number of entries in the history.
 *
 * @param handle Handle to the linenoise instance.
 * @param len Maximum number of entries.
 * @return true on success, false on failure.
 */
bool linenoise_history_set_max_len(linenoise_handle_t handle, int len);

/**
 * @brief Frees the history associated with the instance.
 *
 * @param handle Handle to the linenoise instance.
 */
void linenoise_history_free(linenoise_handle_t handle);

/**
 * @brief Clears the terminal screen for the instance.
 *
 * @param handle Handle to the linenoise instance.
 */
void linenoise_clear_screen(linenoise_handle_t handle);

/**
 * @brief Sets whether an empty line is allowed to be returned.
 *
 * @param handle Handle to the linenoise instance.
 * @param empty_line true to allow empty lines, false to disallow.
 */
void linenoise_set_empty_line(linenoise_handle_t handle, bool empty_line);

/**
 * @brief Checks whether the instance allows returning an empty line.
 *
 * @param handle Handle to the linenoise instance.
 * @return true if empty lines are allowed, false otherwise.
 */
bool linenoise_is_empty_line(linenoise_handle_t handle);

/**
 * @brief Enables or disables multi-line editing.
 *
 * @param handle Handle to the linenoise instance.
 * @param multi_line true to enable multi-line input, false to disable.
 */
void linenoise_set_multi_line(linenoise_handle_t handle, bool multi_line);

/**
 * @brief Checks if multi-line editing is enabled.
 *
 * @param handle Handle to the linenoise instance.
 * @return true if multi-line input is enabled, false otherwise.
 */
bool linenoise_is_multi_line(linenoise_handle_t handle);

/**
 * @brief Enables or disables dumb mode (disables line editing features).
 *
 * @param handle Handle to the linenoise instance.
 * @param dumb_mode true to enable dumb mode, false to disable.
 */
void linenoise_set_dumb_mode(linenoise_handle_t handle, bool dumb_mode);

/**
 * @brief Checks if dumb mode is enabled.
 *
 * @param handle Handle to the linenoise instance.
 * @return true if dumb mode is enabled, false otherwise.
 */
bool linenoise_is_dumb_mode(linenoise_handle_t handle);

/**
 * @brief Sets the maximum length of the command line buffer.
 *
 * @param handle Handle to the linenoise instance.
 * @param length Maximum length in bytes.
 */
void linenoise_set_max_cmd_line_length(linenoise_handle_t handle, size_t length);

/**
 * @brief Gets the maximum allowed command line buffer length.
 *
 * @param handle Handle to the linenoise instance.
 * @return Maximum command line length in bytes.
 */
size_t linenoise_get_max_cmd_line_length(linenoise_handle_t handle);

/*------------------------------------------------------------------------------------------------
 *OLD LINENOISE API
 *------------------------------------------------------------------------------------------------*/

typedef struct linenoise_completions linenoiseCompletions;

typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
typedef char*(linenoiseHintsCallback)(const char *, int *color, int *bold);
typedef void(linenoiseFreeHintsCallback)(void *);

void linenoiseSetCompletionCallback(linenoiseCompletionCallback *);
void linenoiseSetHintsCallback(linenoiseHintsCallback *);
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *);
void linenoiseAddCompletion(linenoiseCompletions *, const char *);
void linenoiseSetReadFunction(linenoise_read_bytes_fn read_fn);
void linenoiseSetWriteFunction(linenoise_write_bytes_fn write_fn);
void linenoiseSetReadCharacteristics(void);

int linenoiseProbe(void);
char *linenoise(const char *prompt);
void linenoiseFree(void *ptr);
int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);
void linenoiseHistoryFree(void);
void linenoiseClearScreen(void);
void linenoiseSetMultiLine(int ml);
void linenoiseSetDumbMode(int set);
bool linenoiseIsDumbMode(void);
void linenoisePrintKeyCodes(void);
void linenoiseAllowEmpty(bool);
int linenoiseSetMaxLineLen(size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __LINENOISE_H */
