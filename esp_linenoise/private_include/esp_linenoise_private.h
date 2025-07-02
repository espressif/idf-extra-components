/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include "esp_linenoise.h"

#define ESP_LINENOISE_DEFAULT_PROMPT ">"
#define ESP_LINENOISE_DEFAULT_HISTORY_MAX_LENGTH 100
#define ESP_LINENOISE_DEFAULT_MAX_LINE 4096
#define ESP_LINENOISE_MINIMAL_MAX_LINE 64
#define ESP_LINENOISE_COMMAND_MAX_LEN 32
#define ESP_LINENOISE_PASTE_KEY_DELAY 30 /* Delay, in milliseconds, between two characters being pasted from clipboard */

enum KEY_ACTION {
    KEY_NULL = 0,       /* NULL */
    CTRL_A = 1,         /* Ctrl+a */
    CTRL_B = 2,         /* Ctrl-b */
    CTRL_C = 3,         /* Ctrl-c */
    CTRL_D = 4,         /* Ctrl-d */
    CTRL_E = 5,         /* Ctrl-e */
    CTRL_F = 6,         /* Ctrl-f */
    CTRL_H = 8,         /* Ctrl-h */
    TAB = 9,            /* Tab */
    CTRL_K = 11,        /* Ctrl+k */
    CTRL_L = 12,        /* Ctrl+l */
    ENTER = 10,         /* Enter */
    CTRL_N = 14,        /* Ctrl-n */
    CTRL_P = 16,        /* Ctrl-p */
    CTRL_T = 20,        /* Ctrl-t */
    CTRL_U = 21,        /* Ctrl+u */
    CTRL_W = 23,        /* Ctrl+w */
    ESC = 27,           /* Escape */
    UNIT_SEP = 31,      /* ctrl-_ */
    BACKSPACE =  127    /* Backspace */
};

typedef struct esp_linenoise_state {
    char *buffer; /* Edited line buffer. */
    size_t buffer_length; /* Edited line buffer size. */
    size_t prompt_length; /* Prompt length. */
    size_t cur_cursor_position; /* Current cursor position. */
    size_t old_cursor_position; /* Previous refresh cursor position. */
    size_t len; /* Current edited line length. */
    size_t columns; /* Number of columns in terminal. */
    size_t max_rows_used; /* Maximum num of rows used so far (multiline mode) */
    int history_index; /* The history index we are currently editing. */
    int history_length; /* The current length of the history*/
} esp_linenoise_state_t;

typedef struct esp_linenoise_instance {
    esp_linenoise_handle_t self;
    esp_linenoise_config_t config;
    esp_linenoise_state_t state;
} esp_linenoise_instance_t;

/**
 * @brief Stores a list of completion strings.
 */
typedef struct esp_linenoise_completions {
    size_t len;   /**< Number of completions. */
    char **cvec;  /**< Array of completion strings. */
} esp_linenoise_completions_t;


inline __attribute__((always_inline))
esp_linenoise_instance_t *esp_linenoise_create_instance_static(void)
{
    esp_linenoise_instance_t *instance = malloc(sizeof(esp_linenoise_instance_t));
    assert(instance != NULL);

    instance->self = instance;
    esp_linenoise_get_instance_config_default(&instance->config);

    /* set the state part of the esp_linenoise_instance_t to 0 to init all values to 0 (or NULL) */
    memset(&instance->state, 0x00, sizeof(esp_linenoise_state_t));

    return instance;
}

/**
 * @brief Probe the terminal to check weather it supports escape sequences
 *
 * @param instance The linenoise instance used to check
 * @return int 0 if the terminal supports escape sequences
 */
int esp_linenoise_probe(esp_linenoise_instance_t *instance);

/**
 * @brief This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example.
 *
 * @param ctx opaque pointer interpreted in line completion structure being
 * filled by the function
 * @param str completed command to add to lc
 */
void esp_linenoise_add_completion(void *ctx, const char *str);

#ifdef __cplusplus
}
#endif
