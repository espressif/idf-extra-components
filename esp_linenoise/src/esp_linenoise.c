/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "sdkconfig.h"
#if !CONFIG_IDF_TARGET_LINUX
// On Linux, we don't need __fbufsize (see comments below), and
// __fbufsize not available on MacOS (which is also considered "Linux" target)
#include <stdio_ext.h> // for __fbufsize
#endif // !CONFIG_IDF_TARGET_LINUX
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <assert.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_linenoise.h"
#include "esp_linenoise_private.h"

static ssize_t esp_linenoise_default_write_bytes(int fd, const void *buf, size_t count)
{
    const int nb_bytes_written = write(fd, buf, count);
    if (nb_bytes_written == count) {
        fsync(fd);
    }
    return nb_bytes_written;
}

__attribute__((weak)) ssize_t esp_linenoise_default_read_bytes(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)state->len,(int)state->cur_cursor_position,(int)state->old_cursor_position,prompt_length,rows,rpos, \
            (int)state->max_rows_used,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
typedef struct append_buffer {
    char *b;
    int len;
} append_buffer_t;

static void esp_linenoise_append_buffer_init(append_buffer_t *ab)
{
    ab->b = NULL;
    ab->len = 0;
}

static void esp_linenoise_append_buffer_append(append_buffer_t *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        return;
    }
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

static void esp_linenoise_append_buffer_free(append_buffer_t *ab)
{
    free(ab->b);
}

/* Helper of esp_linenoise_refresh_single_line() and esp_linenoise_refresh_multi_line() to show hints
 * to the right of the prompt. */
static void esp_linenoise_refresh_show_hints(append_buffer_t *ab, esp_linenoise_instance_t *instance)
{

    esp_linenoise_state_t state = instance->state;
    esp_linenoise_config_t config = instance->config;
    char seq[64];

    if (config.hints_cb && state.prompt_length + state.len < state.columns) {

        int color = -1, bold = 0;
        char *hint = config.hints_cb(state.buffer, &color, &bold);
        if (hint) {
            int hintlen = strlen(hint);
            int hintmaxlen = state.columns - (state.prompt_length + state.len);

            if (hintlen > hintmaxlen) {
                hintlen = hintmaxlen;
            }

            if (bold == 1 && color == -1) {
                color = 37;
            }

            if (color != -1 || bold != 0) {
                snprintf(seq, 64, "\033[%d;%d;49m", bold, color);
                esp_linenoise_append_buffer_append(ab, seq, strlen(seq));
            }

            esp_linenoise_append_buffer_append(ab, hint, hintlen);

            if (color != -1 || bold != 0) {
                esp_linenoise_append_buffer_append(ab, "\033[0m", 4);
            }

            /* Call the function to free the hint returned. */
            if (config.free_hints_cb) {
                config.free_hints_cb(hint);
            }
        }
    }
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void esp_linenoise_refresh_single_line(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    char seq[64];
    size_t prompt_length = state->prompt_length;
    int fd = instance->config.out_fd;
    char *buf = state->buffer;
    size_t len = state->len;
    size_t cur_cursor_position = state->cur_cursor_position;
    append_buffer_t ab;

    while ((prompt_length + cur_cursor_position) >= state->columns) {
        buf++;
        len--;
        cur_cursor_position--;
    }
    while (prompt_length + len > state->columns) {
        len--;
    }

    esp_linenoise_append_buffer_init(&ab);
    /* Cursor to left edge */
    snprintf(seq, 64, "\r");
    esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    /* Write the prompt and the current buffer content */
    esp_linenoise_append_buffer_append(&ab, config->prompt, strlen(config->prompt));
    esp_linenoise_append_buffer_append(&ab, buf, len);
    /* Show hits if any. */
    esp_linenoise_refresh_show_hints(&ab, instance);
    /* Erase to right */
    snprintf(seq, 64, "\x1b[0K");
    esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    /* Move cursor to original position. */
    snprintf(seq, 64, "\r\x1b[%dC", (int)(cur_cursor_position + prompt_length));
    esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    if (config->write_bytes_cb(fd, ab.b, ab.len) == -1) {} /* Can't recover from write error. */
    esp_linenoise_append_buffer_free(&ab);
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void esp_linenoise_refresh_multi_line(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    char seq[64];
    int prompt_length = state->prompt_length;
    int rows = (prompt_length + state->len + state->columns - 1) / state->columns; /* rows used by current buf. */
    int rpos = (prompt_length + state->old_cursor_position + state->columns) / state->columns; /* cursor relative row. */
    int rpos2; /* rpos after refresh. */
    int col; /* column position, zero-based. */
    int old_rows = state->max_rows_used;
    int j;
    int fd = instance->config.out_fd;
    append_buffer_t ab;

    /* Update max_rows_used if needed. */
    if (rows > (int)state->max_rows_used) {
        state->max_rows_used = rows;
    }

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    esp_linenoise_append_buffer_init(&ab);
    if (old_rows - rpos > 0) {
        lndebug("go down %d", old_rows - rpos);
        snprintf(seq, 64, "\x1b[%dB", old_rows - rpos);
        esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    }

    /* Now for every row clear it, go up. */
    for (j = 0; j < old_rows - 1; j++) {
        lndebug("clear+up");
        snprintf(seq, 64, "\r\x1b[0K\x1b[1A");
        esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    }

    /* Clean the top line. */
    lndebug("clear");
    snprintf(seq, 64, "\r\x1b[0K");
    esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));

    /* Write the prompt and the current buffer content */
    esp_linenoise_append_buffer_append(&ab, config->prompt, strlen(config->prompt));
    esp_linenoise_append_buffer_append(&ab, state->buffer, state->len);

    /* Show hits if any. */
    esp_linenoise_refresh_show_hints(&ab, instance);

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (state->cur_cursor_position &&
            state->cur_cursor_position == state->len &&
            (state->cur_cursor_position + prompt_length) % state->columns == 0) {
        lndebug("<newline>");
        esp_linenoise_append_buffer_append(&ab, "\n", 1);
        snprintf(seq, 64, "\r");
        esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
        rows++;
        if (rows > (int)state->max_rows_used) {
            state->max_rows_used = rows;
        }
    }

    /* Move cursor to right position. */
    rpos2 = (prompt_length + state->cur_cursor_position + state->columns) / state->columns; /* current cursor relative row. */
    lndebug("rpos2 %d", rpos2);

    /* Go up till we reach the expected position. */
    if (rows - rpos2 > 0) {
        lndebug("go-up %d", rows - rpos2);
        snprintf(seq, 64, "\x1b[%dA", rows - rpos2);
        esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));
    }

    /* Set column. */
    col = (prompt_length + (int)state->cur_cursor_position) % (int)state->columns;
    lndebug("set col %d", 1 + col);
    if (col) {
        snprintf(seq, 64, "\r\x1b[%dC", col);
    } else {
        snprintf(seq, 64, "\r");
    }
    esp_linenoise_append_buffer_append(&ab, seq, strlen(seq));

    lndebug("\n");
    state->old_cursor_position = state->cur_cursor_position;

    if (config->write_bytes_cb(fd, ab.b, ab.len) == -1) {} /* Can't recover from write error. */
    esp_linenoise_append_buffer_free(&ab);
}

/* Calls the two low level functions esp_linenoise_refresh_single_line() or
 * esp_linenoise_refresh_multi_line() according to the selected mode. */
static void esp_linenoise_refresh_line(esp_linenoise_instance_t *instance)
{
    if (instance->config.allow_multi_line) {
        esp_linenoise_refresh_multi_line(instance);
    } else {
        esp_linenoise_refresh_single_line(instance);
    }
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int esp_linenoise_get_cursor_position(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;

    char buf[ESP_LINENOISE_COMMAND_MAX_LEN] = { 0 };
    int columns = 0;
    int rows = 0;
    int i = 0;
    const int out_fd = instance->config.out_fd;
    const int in_fd = instance->config.in_fd;
    /* The following ANSI escape sequence is used to get from the TTY the
     * cursor position. */
    const char get_cursor_cmd[] = "\x1b[6n";

    /* Send the command to the TTY on the other end of the UART.
     * Let's use unistd's write function. Thus, data sent through it are raw
     * reducing the overhead compared to using fputs, fprintf, etc... */
    const int num_written = config->write_bytes_cb(out_fd, get_cursor_cmd, sizeof(get_cursor_cmd));
    if (num_written != sizeof(get_cursor_cmd)) {
        return -1;
    }

    /* The other end will send its response which format is ESC [ rows ; columns R
     * We don't know exactly how many bytes we have to read, thus, perform a
     * read for each byte.
     * Stop right before the last character of the buffer, to be able to NULL
     * terminate it. */
    while (i < sizeof(buf) - 1) {
        /* Keep using unistd's functions. Here, using `read` instead of `fgets`
         * or `fgets` guarantees us that we we can read a byte regardless on
         * whether the sender sent end of line character(s) (CR, CRLF, LF). */
        if (config->read_bytes_cb(in_fd, buf + i, 1) != 1 || buf[i] == 'R') {
            /* If we couldn't read a byte from in_fd or if 'R' was received,
             * the transmission is finished. */
            break;
        }

        /* For some reasons, it is possible that we receive new line character
         * after querying the cursor position on some UART. Let's ignore them,
         * this will not affect the rest of the program. */
        if (buf[i] != '\n') {
            i++;
        }
    }

    /* NULL-terminate the buffer, this is required by `sscanf`. */
    buf[i] = '\0';

    /* Parse the received data to get the position of the cursor. */
    if (buf[0] != ESC || buf[1] != '[' || sscanf(buf + 2, "%d;%d", &rows, &columns) != 2) {
        return -1;
    }
    return columns;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int esp_linenoise_get_columns(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;

    int start = 0;
    int columns = 0;
    int written = 0;
    char seq[ESP_LINENOISE_COMMAND_MAX_LEN] = { 0 };
    const int fd = instance->config.out_fd;

    /* The following ANSI escape sequence is used to tell the TTY to move
     * the cursor to the most-right position. */
    const char move_cursor_right[] = "\x1b[999C";
    const size_t cmd_len = sizeof(move_cursor_right);

    /* This one is used to set the cursor position. */
    const char set_cursor_pos[] = "\x1b[%dD";

    /* Get the initial position so we can restore it later. */
    start = esp_linenoise_get_cursor_position(instance);
    if (start == -1) {
        goto failed;
    }

    /* Send the command to go to right margin. Use `write` function instead of
     * `fwrite` for the same reasons explained in `esp_linenoise_get_cursor_position()` */
    if (config->write_bytes_cb(fd, move_cursor_right, cmd_len) != cmd_len) {
        goto failed;
    }

    /* After sending this command, we can get the new position of the cursor,
     * we'd get the size, in columns, of the opened TTY. */
    columns = esp_linenoise_get_cursor_position(instance);
    if (columns == -1) {
        goto failed;
    }

    /* Restore the position of the cursor back. */
    if (columns > start) {
        /* Generate the move cursor command. */
        written = snprintf(seq, ESP_LINENOISE_COMMAND_MAX_LEN, set_cursor_pos, columns - start);

        /* If `written` is equal or bigger than ESP_LINENOISE_COMMAND_MAX_LEN, it
         * means that the output has been truncated because the size provided
         * is too smalstate. */
        assert (written < ESP_LINENOISE_COMMAND_MAX_LEN);

        /* Send the command with `write`, which is not buffered. */
        if (config->write_bytes_cb(fd, seq, written) == -1) {
            /* Can't recover... */
        }
    }
    return columns;

failed:
    return 80;
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void esp_linenoise_make_beep_sound(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;

    char bip_screen_str[] = "\x7";
    (void)config->write_bytes_cb(instance->config.out_fd, bip_screen_str, sizeof(bip_screen_str));
}

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void free_completions(esp_linenoise_completions_t *lc)
{
    size_t i;
    for (i = 0; i < lc->len; i++) {
        free(lc->cvec[i]);
    }
    if (lc->cvec != NULL) {
        free(lc->cvec);
    }
}

/* This is an helper function for esp_linenoise_edit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed esp_linenoise_state
 * structure as described in the structure definition. */
static int esp_linenoise_complete_line(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    esp_linenoise_completions_t lc = { 0, NULL };
    int nread, nwritten;
    char c = 0;
    int in_fd = instance->config.in_fd;

    config->completion_cb(state->buffer, &lc, esp_linenoise_add_completion);
    if (lc.len == 0) {
        esp_linenoise_make_beep_sound(instance);
    } else {
        size_t stop = 0, i = 0;

        while (!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                esp_linenoise_state_t saved = *state;

                state->len = state->cur_cursor_position = strlen(lc.cvec[i]);
                state->buffer = lc.cvec[i];
                esp_linenoise_refresh_line(instance);
                state->len = saved.len;
                state->cur_cursor_position = saved.cur_cursor_position;
                state->buffer = saved.buffer;
            } else {
                esp_linenoise_refresh_line(instance);
            }

            nread = config->read_bytes_cb(in_fd, &c, 1);
            if (nread <= 0) {
                free_completions(&lc);
                return -1;
            }

            switch (c) {
            case TAB: /* tab */
                i = (i + 1) % (lc.len + 1);
                if (i == lc.len) {
                    esp_linenoise_make_beep_sound(instance);
                }
                break;
            case ESC: /* escape */
                /* Re-show original buffer */
                if (i < lc.len) {
                    esp_linenoise_refresh_line(instance);
                }
                stop = 1;
                break;
            default:
                /* Update buffer and return */
                if (i < lc.len) {
                    nwritten = snprintf(state->buffer, state->buffer_length, "%s", lc.cvec[i]);
                    state->len = state->cur_cursor_position = nwritten;
                }
                stop = 1;
                break;
            }
        }
    }

    free_completions(&lc);
    return c; /* Return last read character */
}

/* =========================== Line editing ================================= */

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
static int esp_linenoise_edit_insert(esp_linenoise_instance_t *instance, char c)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    int fd = instance->config.out_fd;
    if (state->len < state->buffer_length) {
        if (state->len == state->cur_cursor_position) {
            state->buffer[state->cur_cursor_position] = c;
            state->cur_cursor_position++;
            state->len++;
            state->buffer[state->len] = '\0';
            if ((!config->allow_multi_line && state->prompt_length + state->len < state->columns && !config->hints_cb)) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (config->write_bytes_cb(fd, &c, 1) == -1) {
                    return -1;
                }
            } else {
                esp_linenoise_refresh_line(instance);
            }
        } else {
            memmove(state->buffer + state->cur_cursor_position + 1, state->buffer + state->cur_cursor_position, state->len - state->cur_cursor_position);
            state->buffer[state->cur_cursor_position] = c;
            state->len++;
            state->cur_cursor_position++;
            state->buffer[state->len] = '\0';
            esp_linenoise_refresh_line(instance);
        }
    }
    return 0;
}

static int esp_linenoise_insert_pasted_char(esp_linenoise_instance_t *instance, char c)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;
    int fd = instance->config.out_fd;

    if (state->len < state->buffer_length && state->len == state->cur_cursor_position) {
        state->buffer[state->cur_cursor_position] = c;
        state->cur_cursor_position++;
        state->len++;
        state->buffer[state->len] = '\0';
        if (config->write_bytes_cb(fd, &c, 1) == -1) {
            return -1;
        }
    }
    return 0;
}

/* Move cursor on the left. */
static void esp_linenoise_edit_move_left(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;
    if (state->cur_cursor_position > 0) {
        state->cur_cursor_position--;
        esp_linenoise_refresh_line(instance);
    }
}

/* Move cursor on the right. */
static void esp_linenoise_edit_move_right(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;
    if (state->cur_cursor_position != state->len) {
        state->cur_cursor_position++;
        esp_linenoise_refresh_line(instance);
    }
}

/* Move cursor to the start of the line. */
static void esp_linenoise_edit_move_home(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;
    if (state->cur_cursor_position != 0) {
        state->cur_cursor_position = 0;
        esp_linenoise_refresh_line(instance);
    }
}

/* Move cursor to the end of the line. */
static void esp_linenoise_edit_move_end(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;
    if (state->cur_cursor_position != state->len) {
        state->cur_cursor_position = state->len;
        esp_linenoise_refresh_line(instance);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
#define esp_LINENOISE_HISTORY_NEXT 0
#define esp_LINENOISE_HISTORY_PREV 1
static void esp_linenoise_edit_history_next(esp_linenoise_instance_t *instance, int dir)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    if (state->history_length - state->history_index >= 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        char *buffer_copy = strdup(state->buffer);
        if (!buffer_copy) {
            return;
        }
        free(config->history[state->history_index]);
        config->history[state->history_index] = buffer_copy;
        /* Show the new entry */
        state->history_index += (dir == esp_LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (state->history_index < 0) {
            state->history_index = 0;
            return;
        } else if (state->history_index >= state->history_length) {
            state->history_index = state->history_length - 1;
            return;
        }
        strncpy(state->buffer, config->history[state->history_index], state->buffer_length);
        state->buffer[state->buffer_length - 1] = '\0';
        state->len = state->cur_cursor_position = strlen(state->buffer);
        esp_linenoise_refresh_line(instance);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
static void esp_linenoise_edit_delete(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;

    if (state->len > 0 && state->cur_cursor_position < state->len) {
        memmove(state->buffer + state->cur_cursor_position, state->buffer + state->cur_cursor_position + 1, state->len - state->cur_cursor_position - 1);
        state->len--;
        state->buffer[state->len] = '\0';
        esp_linenoise_refresh_line(instance);
    }
}

/* Backspace implementation. */
static void esp_linenoise_edit_backspace(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;

    if (state->cur_cursor_position > 0 && state->len > 0) {
        memmove(state->buffer + state->cur_cursor_position - 1, state->buffer + state->cur_cursor_position, state->len - state->cur_cursor_position);
        state->cur_cursor_position--;
        state->len--;
        state->buffer[state->len] = '\0';
        esp_linenoise_refresh_line(instance);
    }
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. */
static void esp_linenoise_edit_delete_prev_word(esp_linenoise_instance_t *instance)
{
    esp_linenoise_state_t *state = &instance->state;

    size_t old_pos = state->cur_cursor_position;
    size_t diff;

    while (state->cur_cursor_position > 0 && state->buffer[state->cur_cursor_position - 1] == ' ') {
        state->cur_cursor_position--;
    }
    while (state->cur_cursor_position > 0 && state->buffer[state->cur_cursor_position - 1] != ' ') {
        state->cur_cursor_position--;
    }
    diff = old_pos - state->cur_cursor_position;
    memmove(state->buffer + state->cur_cursor_position, state->buffer + old_pos, state->len - old_pos + 1);
    state->len -= diff;
    esp_linenoise_refresh_line(instance);
}

static uint32_t get_millis(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline size_t esp_linenoise_prompt_len_ignore_escape_seq(const char *prompt)
{
    size_t prompt_length = 0;
    bool in_escape_sequence = false;

    for (; *prompt != '\0'; ++prompt) {
        if (*prompt == '\033') {
            in_escape_sequence = true;
        } else if (in_escape_sequence && *prompt == 'm') {
            in_escape_sequence = false;
        } else if (!in_escape_sequence) {
            ++prompt_length;
        }
    }

    return prompt_length;
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user type enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer. */
static int esp_linenoise_edit(esp_linenoise_instance_t *instance, char *buffer, size_t buffer_length)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    uint32_t t1 = 0;
    int out_fd = instance->config.out_fd;
    int in_fd = instance->config.in_fd;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    state->buffer = buffer;
    state->buffer_length = buffer_length;
    state->prompt_length = strlen(config->prompt);
    state->old_cursor_position = state->cur_cursor_position = 0;
    state->len = 0;
    state->columns = esp_linenoise_get_columns(instance);
    state->max_rows_used = 0;
    state->history_index = 0;

    /* Buffer starts empty. */
    state->buffer[0] = '\0';
    state->buffer_length--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    const esp_err_t ret_val = esp_linenoise_history_add(instance, "");
    if (ret_val != ESP_OK) {
        return -1;
    }

    if (config->write_bytes_cb(out_fd, config->prompt, state->prompt_length) == -1) {
        return -1;
    }

    /* If the prompt has been registered with ANSI escape sequences
     * for terminal colors then we remove them from the prompt length
     * calculation. */
    state->prompt_length = esp_linenoise_prompt_len_ignore_escape_seq(config->prompt);

    while (1) {
        char c;

        /**
         * To determine whether the user is pasting data or typing itself, we
         * need to calculate how many milliseconds elapsed between two key
         * presses. Indeed, if there is less than ESP_LINENOISE_PASTE_KEY_DELAY
         * (typically 30-40ms), then a paste is being performed, else, the
         * user is typing.
         * NOTE: pressing a key down without releasing it will also spend
         * about 40ms (or even more)
         */
        t1 = get_millis();
        int nread = config->read_bytes_cb(in_fd, &c, 1);
        if (nread <= 0) {
            return state->len;
        }

        if ( (get_millis() - t1) < ESP_LINENOISE_PASTE_KEY_DELAY && c != ENTER) {
            /* Pasting data, insert characters without formatting.
            * This can only be performed when the cursor is at the end of the
            * line. */
            if (esp_linenoise_insert_pasted_char(instance, c)) {
                return -1;
            }
            continue;
        }

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && config->completion_cb != NULL) {
            int c2 = esp_linenoise_complete_line(instance);
            /* Return on errors */
            if (c2 < 0) {
                return state->len;
            }
            /* Read next character when 0 */
            if (c2 == 0) {
                continue;
            }
            c = c2;
        }

        switch (c) {
        case ENTER:    /* enter */
            state->history_length--;
            free(config->history[state->history_length]);
            if (config->allow_multi_line) {
                esp_linenoise_edit_move_end(instance);
            }
            if (config->hints_cb) {
                /* Force a refresh without hints to leave the previous
                 * line as the user typed it after a newline. */
                esp_linenoise_hints_t hc = config->hints_cb;
                config->hints_cb = NULL;
                esp_linenoise_refresh_line(instance);
                config->hints_cb = hc;
            }
            return (int)state->len;
        case CTRL_C:     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case BACKSPACE:   /* backspace */
        case CTRL_H:     /* ctrl-h */
            esp_linenoise_edit_backspace(instance);
            break;
        case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
            if (state->len > 0) {
                esp_linenoise_edit_delete(instance);
            } else {
                state->history_length--;
                free(config->history[state->history_length]);
                return -1;
            }
            break;
        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (state->cur_cursor_position > 0 && state->cur_cursor_position < state->len) {
                int aux = state->buffer[state->cur_cursor_position - 1];
                state->buffer[state->cur_cursor_position - 1] = state->buffer[state->cur_cursor_position];
                state->buffer[state->cur_cursor_position] = aux;
                if (state->cur_cursor_position != state->len - 1) {
                    state->cur_cursor_position++;
                }
                esp_linenoise_refresh_line(instance);
            }
            break;
        case CTRL_B:     /* ctrl-b */
            esp_linenoise_edit_move_left(instance);
            break;
        case CTRL_F:     /* ctrl-f */
            esp_linenoise_edit_move_right(instance);
            break;
        case CTRL_P:    /* ctrl-p */
            esp_linenoise_edit_history_next(instance, esp_LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N:    /* ctrl-n */
            esp_linenoise_edit_history_next(instance, esp_LINENOISE_HISTORY_NEXT);
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            state->buffer[0] = '\0';
            state->cur_cursor_position = state->len = 0;
            esp_linenoise_refresh_line(instance);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            state->buffer[state->cur_cursor_position] = '\0';
            state->len = state->cur_cursor_position;
            esp_linenoise_refresh_line(instance);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            esp_linenoise_edit_move_home(instance);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            esp_linenoise_edit_move_end(instance);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            (void)esp_linenoise_clear_screen(instance);
            esp_linenoise_refresh_line(instance);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            esp_linenoise_edit_delete_prev_word(instance);
            break;
        case ESC: {     /* escape sequence */
            /* ESC [ sequences. */
            char seq[3];
            int r = config->read_bytes_cb(in_fd, seq, 1);
            if (r != 1) {
                return -1;
            }
            if (seq[0] == '[') {
                int r = config->read_bytes_cb(in_fd, seq + 1, 1);
                if (r != 1) {
                    return -1;
                }
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    r = config->read_bytes_cb(in_fd, seq + 2, 1);
                    if (r != 1) {
                        return -1;
                    }
                    if (seq[2] == '~') {
                        switch (seq[1]) {
                        case '3': /* Delete key. */
                            esp_linenoise_edit_delete(instance);
                            break;
                        }
                    }
                } else {
                    switch (seq[1]) {
                    case 'A': /* Up */
                        esp_linenoise_edit_history_next(instance, esp_LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        esp_linenoise_edit_history_next(instance, esp_LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        esp_linenoise_edit_move_right(instance);
                        break;
                    case 'D': /* Left */
                        esp_linenoise_edit_move_left(instance);
                        break;
                    case 'H': /* Home */
                        esp_linenoise_edit_move_home(instance);
                        break;
                    case 'F': /* End*/
                        esp_linenoise_edit_move_end(instance);
                        break;
                    }
                }
            }
            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                int r = config->read_bytes_cb(in_fd, seq + 1, 1);
                if (r != 1) {
                    return -1;
                }
                switch (seq[1]) {
                case 'H': /* Home */
                    esp_linenoise_edit_move_home(instance);
                    break;
                case 'F': /* End*/
                    esp_linenoise_edit_move_end(instance);
                    break;
                }
            }
            break;
        }
        default:
            if (esp_linenoise_edit_insert(instance, c)) {
                return -1;
            }
            break;
        }
        fsync(instance->config.out_fd);
    }
    return state->len;
}

static int esp_linenoise_raw(esp_linenoise_instance_t *instance, char *buffer, size_t buffer_length)
{
    esp_linenoise_config_t *config = &instance->config;

    int count;

    if (buffer_length == 0) {
        errno = EINVAL;
        return -1;
    }

    count = esp_linenoise_edit(instance, buffer, buffer_length);
    config->write_bytes_cb(config->out_fd, "\n", 1);
    return count;
}

static int esp_linenoise_dumb(esp_linenoise_instance_t *instance, char *buffer, size_t buffer_length)
{
    esp_linenoise_config_t *config = &instance->config;

    config->write_bytes_cb(instance->config.out_fd, config->prompt, strlen(config->prompt));

    size_t count = 0;
    const int in_fd = instance->config.in_fd;
    char c = 'c';

    // leave the last character free so the user can null terminate the string if needed,
    // also, this is to be consistent with esp_linenoise_edit()
    bool exit_loop = false;
    while (!exit_loop) {
        int nread = config->read_bytes_cb(in_fd, &c, 1);
        if (nread < 0) {
            exit_loop = true;
            count = nread;
            continue;
        }
        if (c == '\n') {
            exit_loop = true;
            continue;
        }
        // if the number of bytes are reached, wait for the user to input
        // a new line character to return, just like in esp_linenoise_edit()
        if (count >= buffer_length - 1) {
            continue;
        } else if (c == BACKSPACE || c == CTRL_H) {
            if (count > 0) {
                buffer[count - 1] = 0;
                count--;

                /* Only erase symbol echoed from in_fd. */
                char erase_symbol_str[] = "\x08";
                config->write_bytes_cb(instance->config.out_fd, erase_symbol_str, sizeof(erase_symbol_str)); /* Windows CMD: erase symbol under cursor */
            } else {
                /* Consume backspace if the command line is empty to avoid erasing the prompt */
                continue;
            }

        } else if (c <= UNIT_SEP) {
            /* Consume all character that are non printable (the backspace
             * case is handled above) */
            continue;
        } else {
            buffer[count] = c;
            ++count;
        }
        config->write_bytes_cb(instance->config.out_fd, &c, 1); /* echo */
    }
    config->write_bytes_cb(instance->config.out_fd, "\n", 1);

    // null terminate the string
    buffer[count + 1] = '\0';

    return count;
}

static void esp_linenoise_sanitize(char *src)
{
    char *dst = src;
    for (int c = *src; c != 0; src++, c = *src) {
        if (isprint(c)) {
            *dst = c;
            ++dst;
        }
    }
    *dst = 0;
}

int esp_linenoise_probe(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;

    /* Make sure we are in non blocking mode before performing the terminal probing */
    int fd_in = instance->config.in_fd;
    int out_fd = instance->config.out_fd;
    int old_flags = fcntl(fd_in, F_GETFL);
    int new_flags = old_flags | O_NONBLOCK;
    int res = fcntl(fd_in, F_SETFL, new_flags);
    if (res != 0) {
        return -1;
    }

    /* Device status request */
    char status_request_str[] = "\x1b[5n";
    config->write_bytes_cb(out_fd, status_request_str, sizeof(status_request_str));

    /* Try to read response */
    int timeout_ms = 500;
    const int retry_ms = 10;
    size_t read_bytes = 0;
    while (timeout_ms > 0 && read_bytes < 4) { // response is ESC[0n or ESC[3n
        usleep(retry_ms * 1000);
        timeout_ms -= retry_ms;
        char c;
        int cb = config->read_bytes_cb(fd_in, &c, 1);
        if (cb < 0) {
            continue;
        }
        if (read_bytes == 0 && c != ESC) {
            /* invalid response, try again until the timeout triggers */
            continue;
        }
        read_bytes += cb;
    }

    /* Switch back to whatever mode we had before the function call */
    res = fcntl(fd_in, F_SETFL, old_flags);
    if (res != 0) {
        return -1;
    }

    if (read_bytes < 4) {
        return -2;
    }

    return 0;
}

#define esp_LINENOISE_CHECK_INSTANCE(handle)                                                                    \
    if((handle == NULL) || ((esp_linenoise_instance_t*)handle->self != (esp_linenoise_instance_t*)handle)) {    \
        return ESP_ERR_INVALID_ARG;                                                                             \
}

void esp_linenoise_get_instance_config_default(esp_linenoise_config_t *config)
{
    *config = (esp_linenoise_config_t) {
        .prompt = ESP_LINENOISE_DEFAULT_PROMPT,
        .max_cmd_line_length = ESP_LINENOISE_DEFAULT_MAX_LINE,
        .history_max_length = ESP_LINENOISE_DEFAULT_HISTORY_MAX_LENGTH,
        .in_fd = STDIN_FILENO,
        .out_fd = STDOUT_FILENO,
        .allow_multi_line = false,  /* Multi line mode. Default is single line. */
        .allow_empty_line = true, /* Allow linenoise to return an empty string. On by default */
        .allow_dumb_mode = false, /* Dumb mode where line editing is disabled. Off by default */
        .completion_cb = NULL,
        .hints_cb = NULL,
        .free_hints_cb = NULL,
        .write_bytes_cb = esp_linenoise_default_write_bytes,
        .read_bytes_cb = esp_linenoise_default_read_bytes,
        .history = NULL,
    };
}

esp_err_t esp_linenoise_create_instance(const esp_linenoise_config_t *config, esp_linenoise_handle_t *out_handle)
{
    if (!config || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    /* make sure the history is NULL since the linenoise library will allocate it */
    if (config->history != NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_linenoise_instance_t *instance = malloc(sizeof(esp_linenoise_instance_t));
    if (!instance) {
        return ESP_ERR_NO_MEM;
    }

    instance->config = *config;

    /* set the state part of the esp_linenoise_instance_t to 0 to
     * init all values to 0 (or NULL) */
    memset(&instance->state, 0x00, sizeof(esp_linenoise_state_t));

    if (instance->config.in_fd == -1) {
        instance->config.in_fd = STDIN_FILENO;
    }
    if (instance->config.out_fd == -1) {
        instance->config.out_fd = STDOUT_FILENO;
    }
    if (!instance->config.prompt) {
        instance->config.prompt = ESP_LINENOISE_DEFAULT_PROMPT;
    }
    if (!instance->config.max_cmd_line_length) {
        instance->config.max_cmd_line_length = ESP_LINENOISE_DEFAULT_MAX_LINE;
    }
    if (!instance->config.history_max_length) {
        instance->config.history_max_length = ESP_LINENOISE_DEFAULT_HISTORY_MAX_LENGTH;
    }
    if (instance->config.write_bytes_cb == NULL) {
        instance->config.write_bytes_cb = esp_linenoise_default_write_bytes;
    }
    if ((instance->config.read_bytes_cb == NULL) ||
            (instance->config.read_bytes_cb == esp_linenoise_default_read_bytes)) {
        /*  since we are using the default read function, make sure
         * blocking read are set */
        int flags = fcntl(instance->config.in_fd, F_GETFL, 0);
        flags &= ~O_NONBLOCK;
        fcntl(instance->config.in_fd, F_SETFL, flags);
        instance->config.read_bytes_cb = esp_linenoise_default_read_bytes;

        if (esp_linenoise_set_event_fd != NULL) {
            const esp_err_t ret_val = esp_linenoise_set_event_fd(instance);
            if (ret_val != ESP_OK) {
                free(instance);
                return ret_val;
            }
        } else {
            /* make sure the state->mux is set to NULL */
            instance->state.mux = NULL;
        }
    }

    const int probe_status = esp_linenoise_probe(instance);
    if (probe_status == 0) {
        /* escape sequences supported*/
        instance->config.allow_dumb_mode = false;
    } else {
        /* error during the probing or escape sequences not supported */
        instance->config.allow_dumb_mode = true;

        char buf[256];
        int len = snprintf(buf, sizeof(buf),
                           "\r\n"
                           "Your terminal application does not support escape sequences.\n\n"
                           "Line editing and history features are disabled.\n\n"
                           "On Windows, try using Windows Terminal or Putty instead.\r\n");

        instance->config.write_bytes_cb(instance->config.out_fd, buf, len);
    }

    /* set the self value to the handle of instance and assign the instance to out_handle */
    instance->self = instance;
    *out_handle =  (esp_linenoise_handle_t)instance;

    return ESP_OK;
}

esp_err_t esp_linenoise_delete_instance(esp_linenoise_handle_t handle)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;

    // free the history first since it was allocated by linenoise
    esp_err_t ret_val = esp_linenoise_history_free(handle);
    if (ret_val != ESP_OK) {
        return ret_val;
    }

    // delete the mutex in the state and close the eventfd
    // if it was created
    if (esp_linenoise_remove_event_fd != NULL) {
        ret_val = esp_linenoise_remove_event_fd(instance);
        if (ret_val != ESP_OK) {
            return ret_val;
        }
    }

    // reset the memory
    memset(instance, 0x00, sizeof(esp_linenoise_instance_t));

    // free the instance
    free(instance);

    return ESP_OK;
}

esp_err_t esp_linenoise_get_line(esp_linenoise_handle_t handle, char *cmd_line_buffer, size_t cmd_line_length)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    if (cmd_line_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    if ((cmd_line_length == 0) || (cmd_line_length > config->max_cmd_line_length)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* take the mutex, it will be released only when esp_linenoise_raw
     * or esp_linenoise_dumb returns */
    if (state->mux != NULL) {
        (void)xSemaphoreTake(state->mux, portMAX_DELAY);
    }

    int count = 0;
    if (!config->allow_dumb_mode) {
        count = esp_linenoise_raw(instance, cmd_line_buffer, cmd_line_length);
    } else {
        count = esp_linenoise_dumb(instance, cmd_line_buffer, cmd_line_length);
    }

    esp_err_t ret_val = ESP_OK;
    if (count > 0) {
        esp_linenoise_sanitize(cmd_line_buffer);
    } else if (count == 0 && config->allow_empty_line) {
        /* will return an empty (0-length) string */
    } else {
        ret_val = ESP_FAIL;
    }

    /* release the mutex, signaling that esp_linenoise_get_line returned */
    if (state->mux != NULL) {
        xSemaphoreGive(state->mux);
    }

    return ret_val;
}

void esp_linenoise_add_completion(void *ctx, const char *str)
{
    esp_linenoise_completions_t *lc = (esp_linenoise_completions_t *)ctx;

    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len + 1);
    if (copy == NULL) {
        return;
    }
    memcpy(copy, str, len + 1);
    cvec = realloc(lc->cvec, sizeof(char *) * (lc->len + 1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}

esp_err_t esp_linenoise_history_add(esp_linenoise_handle_t handle, const char *line)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    esp_linenoise_config_t *config = &((esp_linenoise_instance_t *)handle)->config;
    esp_linenoise_state_t *state = &((esp_linenoise_instance_t *)handle)->state;

    if (config->history_max_length == 0) {
        return ESP_ERR_NO_MEM;
    }

    /* Initialization on first calstate. */
    if (config->history == NULL) {
        config->history = malloc(sizeof(char *) * config->history_max_length);
        if (config->history == NULL) {
            return ESP_ERR_NO_MEM;
        }
        memset(config->history, 0, (sizeof(char *) * config->history_max_length));

        state->history_length = 0;
    }

    /* Don't add duplicated lines. */
    if ((state->history_length == 0) ||
            (strcmp(config->history[state->history_length - 1], line) != 0)) {

        /* Add an heap allocated copy of the line in the history.
        * If we reached the max length, remove the older line. */
        char *line_copy = strdup(line);
        if (!line_copy) {
            return ESP_ERR_NO_MEM;
        }
        if (state->history_length == config->history_max_length) {
            free(config->history[0]);
            memmove(config->history, config->history + 1, sizeof(char *) * (config->history_max_length - 1));
            state->history_length--;
        }
        config->history[state->history_length] = line_copy;
        state->history_length++;
    }

    return ESP_OK;
}

esp_err_t esp_linenoise_history_save(esp_linenoise_handle_t handle, const char *filename)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;

    FILE *fp;
    int j;

    fp = fopen(filename, "w");
    if (fp == NULL) {
        return ESP_FAIL;
    }

    for (j = 0; j < instance->state.history_length; j++) {
        fprintf(fp, "%s\n", instance->config.history[j]);
    }

    fclose(fp);
    return ESP_OK;
}

esp_err_t esp_linenoise_history_load(esp_linenoise_handle_t handle, const char *filename)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return ESP_FAIL;
    }

    char *buf = calloc(1, instance->config.max_cmd_line_length);
    if (buf == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    while (fgets(buf, instance->config.max_cmd_line_length, fp) != NULL) {
        char *p;

        p = strchr(buf, '\r');
        if (!p) {
            p = strchr(buf, '\n');
        }
        if (p) {
            *p = '\0';
        }
        const esp_err_t ret_val = esp_linenoise_history_add(handle, buf);
        if (ret_val != ESP_OK) {
            free(buf);
            fclose(fp);
            return ret_val;
        }
    }

    free(buf);
    fclose(fp);

    return ESP_OK;
}

esp_err_t esp_linenoise_history_set_max_len(esp_linenoise_handle_t handle, int new_length)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);

    esp_linenoise_config_t *config = &((esp_linenoise_instance_t *)handle)->config;
    esp_linenoise_state_t *state = &((esp_linenoise_instance_t *)handle)->state;

    if (new_length == config->history_max_length) {
        /* the requested history size is the same as the current
         * one, return ok without changing anything */
        return ESP_OK;
    }
    if (new_length < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    char **new_history;
    if (config->history) {
        int to_copy = state->history_length;

        new_history = malloc(sizeof(char *) * new_length);
        if (new_history == NULL) {
            return ESP_ERR_NO_MEM;
        }

        /* If we can't copy everything, free the elements we'll not use. */
        if (new_length < to_copy) {
            int j;

            for (j = 0; j < to_copy - new_length; j++) {
                free(config->history[j]);
            }
            to_copy = new_length;
        }
        memset(new_history, 0, sizeof(char *) * new_length);
        memcpy(new_history, config->history + (state->history_length - to_copy), sizeof(char *)*to_copy);
        free(config->history);
        config->history = new_history;
    }

    config->history_max_length = new_length;

    if (state->history_length > config->history_max_length) {
        state->history_length = config->history_max_length;
    }
    return ESP_OK;
}

esp_err_t esp_linenoise_history_free(esp_linenoise_handle_t handle)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;

    if (instance->config.history) {
        for (int j = 0; j < instance->state.history_length; j++) {
            free(instance->config.history[j]);
        }
        free(instance->config.history);
    }
    instance->config.history = NULL;
    instance->state.history_length = 0;

    return ESP_OK;
}

esp_err_t esp_linenoise_clear_screen(esp_linenoise_handle_t handle)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    esp_linenoise_instance_t *instance = (esp_linenoise_instance_t *)handle;
    esp_linenoise_config_t *config = &instance->config;

    char erase_screen_str[] = "\x1b[H\x1b[2J";
    size_t msg_size = sizeof(erase_screen_str);
    ssize_t nb_bytes = config->write_bytes_cb(config->out_fd, erase_screen_str, msg_size);
    if (nb_bytes < 0 || nb_bytes != msg_size) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_linenoise_set_empty_line(esp_linenoise_handle_t handle, bool empty_line)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    ((esp_linenoise_instance_t *)handle)->config.allow_empty_line = empty_line;
    return ESP_OK;
}

esp_err_t esp_linenoise_is_empty_line(esp_linenoise_handle_t handle, bool *is_empty_line)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    *is_empty_line = ((esp_linenoise_instance_t *)handle)->config.allow_empty_line;
    return ESP_OK;
}

esp_err_t esp_linenoise_set_multi_line(esp_linenoise_handle_t handle, bool multi_line)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    ((esp_linenoise_instance_t *)handle)->config.allow_multi_line = multi_line;
    return ESP_OK;
}

esp_err_t esp_linenoise_is_multi_line(esp_linenoise_handle_t handle, bool *is_multi_line)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    *is_multi_line = ((esp_linenoise_instance_t *)handle)->config.allow_multi_line;
    return ESP_OK;
}

esp_err_t esp_linenoise_set_dumb_mode(esp_linenoise_handle_t handle, bool dumb_mode)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    ((esp_linenoise_instance_t *)handle)->config.allow_dumb_mode = dumb_mode;
    return ESP_OK;
}

esp_err_t esp_linenoise_is_dumb_mode(esp_linenoise_handle_t handle, bool *is_dumb_mode)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    *is_dumb_mode = ((esp_linenoise_instance_t *)handle)->config.allow_dumb_mode;
    return ESP_OK;
}

esp_err_t esp_linenoise_set_max_cmd_line_length(esp_linenoise_handle_t handle, size_t length)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    if (length >= ESP_LINENOISE_MINIMAL_MAX_LINE) {
        ((esp_linenoise_instance_t *)handle)->config.max_cmd_line_length = length;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t esp_linenoise_get_max_cmd_line_length(esp_linenoise_handle_t handle, size_t *max_cmd_line_length)
{
    esp_LINENOISE_CHECK_INSTANCE(handle);
    *max_cmd_line_length = ((esp_linenoise_instance_t *)handle)->config.max_cmd_line_length;
    return ESP_OK;
}
