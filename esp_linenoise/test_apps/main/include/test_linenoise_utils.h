/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/time.h>

#define CMD_LINE_LENGTH 32 /* set to the value of ESP_LINENOISE_COMMAND_MAX_LEN */

#define COMPOUND_LITERAL(x) ((char[]){(char)x, '\0'})
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

typedef struct command {
    const char *request;
    const char *response;
} command_t;

// list of commands that the linenoise tests need
// to intercept and potentially answer for linenoise to
// behave as expected
const command_t commands[] = {
    {"\x1b[5n", "\x1b[0n"}, // probe for escape sequence support
    {"\x1b[H\x1b[2J", "screen cleared"}, // clear screen request, the response will be analyzed in the concerned test
    {"\x1b[6n", "\x1b[10;50R"}, // request for rows, cols
    {"\x1b[999C", NULL}, // move the cursor right, no response needed
};

inline __attribute__((always_inline))
uint32_t get_millis(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

inline __attribute__((always_inline))
void wait_ms(int ms)
{
    int cur_time = 0;
    const int timeout = get_millis() + ms;
    do {
        cur_time = get_millis();
    } while (cur_time < timeout);
}
