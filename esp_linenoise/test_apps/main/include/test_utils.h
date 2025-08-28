/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

extern const command_t commands[];
extern const size_t commands_count;

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

void test_send_characters(int socket_fd, const char *msg);

#ifdef __cplusplus
}
#endif
