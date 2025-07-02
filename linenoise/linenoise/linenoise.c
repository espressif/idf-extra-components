/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2016, Salvatore Sanfilippo <antirez at gmail dot com>
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
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
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
#include <sys/param.h>
#include <assert.h>
#include "linenoise.h"

#define LINENOISE_DEFAULT_PROMPT ">"
#define LINENOISE_DEFAULT_HISTORY_MAX_LENGTH 100
#define LINENOISE_DEFAULT_MAX_LINE 4096
#define LINENOISE_MINIMAL_MAX_LINE 64
#define LINENOISE_COMMAND_MAX_LEN 32
#define LINENOISE_PASTE_KEY_DELAY 30 /* Delay, in milliseconds, between two characters being pasted from clipboard */

enum KEY_ACTION{
	KEY_NULL = 0,	    /* NULL */
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

static ssize_t linenoise_default_write_bytes(int fd, const void *buf, size_t count)
{
    const int nb_bytes_written = write(fd, buf, count);
    if (nb_bytes_written == count) {
        fsync(fd);
    }
    return nb_bytes_written;
}

typedef struct linenoise_state {
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
} linenoise_instance_state_t;

typedef struct linenoise_instance {
    linenoise_handle_t self;
    linenoise_instance_param_t param;
    linenoise_instance_state_t state;
} linenoise_instance_t;

static linenoise_instance_t *s_linenoise_instance = NULL;

static inline __attribute__((always_inline))
linenoise_instance_t* linenoise_create_instance_static()
{
    linenoise_instance_t *instance = malloc(sizeof(linenoise_instance_t));
    assert(instance != NULL);

    instance->self = instance;
    instance->param = linenoise_get_instance_param_default();

    /* set the state part of the linenoise_instance_t to 0 to init all values to 0 (or NULL) */
    memset(&instance->state, 0x00, sizeof(linenoise_instance_state_t));

    return instance;
}

static inline __attribute__((always_inline))
linenoise_instance_t* linenoise_get_static_instance()
{
    if (!s_linenoise_instance) {
        s_linenoise_instance = linenoise_create_instance_static();
    }
    return s_linenoise_instance;
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
            (int)l->len,(int)l->cur_cursor_position,(int)l->old_cursor_position,prompt_length,rows,rpos, \
            (int)l->max_rows_used,old_rows); \
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

static void linenoise_append_buffer_init(append_buffer_t *ab)
{
    ab->b = NULL;
    ab->len = 0;
}

static void linenoise_append_buffer_append(append_buffer_t *ab, const char *s, int len)
{
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

static void linenoise_append_buffer_free(append_buffer_t *ab)
{
    free(ab->b);
}

/* Helper of linenoise_refresh_single_line() and linenoise_refresh_multi_line() to show hints
 * to the right of the prompt. */
static void linenoise_refresh_show_hints(append_buffer_t *ab, linenoise_instance_t *instance)
{

    linenoise_instance_state_t l = instance->state;
    linenoise_instance_param_t p = instance->param;
    char seq[64];

    if (p.hints_cb && l.prompt_length + l.len < l.columns) {
        int color = -1, bold = 0;
        char *hint = p.hints_cb(l.buffer, &color, &bold);
        if (hint) {
            int hintlen = strlen(hint);
            int hintmaxlen = l.columns - (l.prompt_length + l.len);

            if (hintlen > hintmaxlen) {
                hintlen = hintmaxlen;
            }

            if (bold == 1 && color == -1) {
                color = 37;
            }

            if (color != -1 || bold != 0) {
                snprintf(seq, 64, "\033[%d;%d;49m", bold, color);
                linenoise_append_buffer_append(ab, seq, strlen(seq));
            }

            linenoise_append_buffer_append(ab, hint, hintlen);

            if (color != -1 || bold != 0) {
                linenoise_append_buffer_append(ab, "\033[0m", 4);
            }

            /* Call the function to free the hint returned. */
            if (p.free_hints_cb) {
                p.free_hints_cb(hint);
            }
        }
    }
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void linenoise_refresh_single_line(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    char seq[64];
    size_t prompt_length = l->prompt_length;
    int fd = instance->param.out_fd;
    char *buf = l->buffer;
    size_t len = l->len;
    size_t cur_cursor_position = l->cur_cursor_position;
    append_buffer_t ab;

    while((prompt_length+cur_cursor_position) >= l->columns) {
        buf++;
        len--;
        cur_cursor_position--;
    }
    while (prompt_length+len > l->columns){
        len--;
    }

    linenoise_append_buffer_init(&ab);
    /* Cursor to left edge */
    snprintf(seq,64,"\r");
    linenoise_append_buffer_append(&ab,seq,strlen(seq));
    /* Write the prompt and the current buffer content */
    linenoise_append_buffer_append(&ab,p->prompt,strlen(p->prompt));
    linenoise_append_buffer_append(&ab,buf,len);
    /* Show hits if any. */
    linenoise_refresh_show_hints(&ab, instance);
    /* Erase to right */
    snprintf(seq,64,"\x1b[0K");
    linenoise_append_buffer_append(&ab,seq,strlen(seq));
    /* Move cursor to original position. */
    snprintf(seq,64,"\r\x1b[%dC", (int)(cur_cursor_position+prompt_length));
    linenoise_append_buffer_append(&ab,seq,strlen(seq));
    if (p->write_bytes_fn(fd, ab.b, ab.len) == -1) {} /* Can't recover from write error. */
    linenoise_append_buffer_free(&ab);
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void linenoise_refresh_multi_line(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    char seq[64];
    int prompt_length = l->prompt_length;
    int rows = (prompt_length+l->len+l->columns-1)/l->columns; /* rows used by current buf. */
    int rpos = (prompt_length+l->old_cursor_position+l->columns)/l->columns; /* cursor relative row. */
    int rpos2; /* rpos after refresh. */
    int col; /* column position, zero-based. */
    int old_rows = l->max_rows_used;
    int j;
    int fd = instance->param.out_fd;
    append_buffer_t ab;

    /* Update max_rows_used if needed. */
    if (rows > (int)l->max_rows_used) l->max_rows_used = rows;

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    linenoise_append_buffer_init(&ab);
    if (old_rows-rpos > 0) {
        lndebug("go down %d", old_rows-rpos);
        snprintf(seq,64,"\x1b[%dB", old_rows-rpos);
        linenoise_append_buffer_append(&ab,seq,strlen(seq));
    }

    /* Now for every row clear it, go up. */
    for (j = 0; j < old_rows-1; j++) {
        lndebug("clear+up");
        snprintf(seq,64,"\r\x1b[0K\x1b[1A");
        linenoise_append_buffer_append(&ab,seq,strlen(seq));
    }

    /* Clean the top line. */
    lndebug("clear");
    snprintf(seq,64,"\r\x1b[0K");
    linenoise_append_buffer_append(&ab,seq,strlen(seq));

    /* Write the prompt and the current buffer content */
    linenoise_append_buffer_append(&ab,p->prompt,strlen(p->prompt));
    linenoise_append_buffer_append(&ab,l->buffer,l->len);

    /* Show hits if any. */
    linenoise_refresh_show_hints(&ab, instance);

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (l->cur_cursor_position &&
        l->cur_cursor_position == l->len &&
        (l->cur_cursor_position+prompt_length) % l->columns == 0)
    {
        lndebug("<newline>");
        linenoise_append_buffer_append(&ab,"\n",1);
        snprintf(seq,64,"\r");
        linenoise_append_buffer_append(&ab,seq,strlen(seq));
        rows++;
        if (rows > (int)l->max_rows_used) l->max_rows_used = rows;
    }

    /* Move cursor to right position. */
    rpos2 = (prompt_length+l->cur_cursor_position+l->columns)/l->columns; /* current cursor relative row. */
    lndebug("rpos2 %d", rpos2);

    /* Go up till we reach the expected position. */
    if (rows-rpos2 > 0) {
        lndebug("go-up %d", rows-rpos2);
        snprintf(seq,64,"\x1b[%dA", rows-rpos2);
        linenoise_append_buffer_append(&ab,seq,strlen(seq));
    }

    /* Set column. */
    col = (prompt_length+(int)l->cur_cursor_position) % (int)l->columns;
    lndebug("set col %d", 1+col);
    if (col)
        snprintf(seq,64,"\r\x1b[%dC", col);
    else
        snprintf(seq,64,"\r");
    linenoise_append_buffer_append(&ab,seq,strlen(seq));

    lndebug("\n");
    l->old_cursor_position = l->cur_cursor_position;

    if (p->write_bytes_fn(fd,ab.b,ab.len) == -1) {} /* Can't recover from write error. */
    linenoise_append_buffer_free(&ab);
}

/* Calls the two low level functions linenoise_refresh_single_line() or
 * linenoise_refresh_multi_line() according to the selected mode. */
static void linenoise_refresh_line(linenoise_instance_t *instance)
{
    if (instance->param.allow_multi_line) {
        linenoise_refresh_multi_line(instance);
    }
    else {
        linenoise_refresh_single_line(instance);
    }
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int linenoise_get_cursor_position(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;

    char buf[LINENOISE_COMMAND_MAX_LEN] = { 0 };
    int columns = 0;
    int rows = 0;
    int i = 0;
    const int out_fd = instance->param.out_fd;
    const int in_fd = instance->param.in_fd;
    /* The following ANSI escape sequence is used to get from the TTY the
     * cursor position. */
    const char get_cursor_cmd[] = "\x1b[6n";

    /* Send the command to the TTY on the other end of the UART.
     * Let's use unistd's write function. Thus, data sent through it are raw
     * reducing the overhead compared to using fputs, fprintf, etc... */
    const int num_written = p->write_bytes_fn(out_fd, get_cursor_cmd, sizeof(get_cursor_cmd));
    if (num_written != sizeof(get_cursor_cmd)) {
        return -1;
    }

    /* The other end will send its response which format is ESC [ rows ; columns R
     * We don't know exactly how many bytes we have to read, thus, perform a
     * read for each byte.
     * Stop right before the last character of the buffer, to be able to NULL
     * terminate it. */
    while (i < sizeof(buf)-1) {
        /* Keep using unistd's functions. Here, using `read` instead of `fgets`
         * or `fgets` guarantees us that we we can read a byte regardless on
         * whether the sender sent end of line character(s) (CR, CRLF, LF). */
        if (p->read_bytes_fn(in_fd, buf + i, 1) != 1 || buf[i] == 'R') {
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
    if (buf[0] != ESC || buf[1] != '[' || sscanf(buf+2,"%d;%d",&rows,&columns) != 2) {
        return -1;
    }
    return columns;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int linenoise_get_columns(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;

    int start = 0;
    int columns = 0;
    int written = 0;
    char seq[LINENOISE_COMMAND_MAX_LEN] = { 0 };
    const int fd = instance->param.out_fd;

    /* The following ANSI escape sequence is used to tell the TTY to move
     * the cursor to the most-right position. */
    const char move_cursor_right[] = "\x1b[999C";
    const size_t cmd_len = sizeof(move_cursor_right);

    /* This one is used to set the cursor position. */
    const char set_cursor_pos[] = "\x1b[%dD";

    /* Get the initial position so we can restore it later. */
    start = linenoise_get_cursor_position(instance);
    if (start == -1) {
        goto failed;
    }

    /* Send the command to go to right margin. Use `write` function instead of
     * `fwrite` for the same reasons explained in `linenoise_get_cursor_position()` */
    if (p->write_bytes_fn(fd, move_cursor_right, cmd_len) != cmd_len) {
        goto failed;
    }

    /* After sending this command, we can get the new position of the cursor,
     * we'd get the size, in columns, of the opened TTY. */
    columns = linenoise_get_cursor_position(instance);
    if (columns == -1) {
        goto failed;
    }

    /* Restore the position of the cursor back. */
    if (columns > start) {
        /* Generate the move cursor command. */
        written = snprintf(seq, LINENOISE_COMMAND_MAX_LEN, set_cursor_pos, columns-start);

        /* If `written` is equal or bigger than LINENOISE_COMMAND_MAX_LEN, it
         * means that the output has been truncated because the size provided
         * is too small. */
        assert (written < LINENOISE_COMMAND_MAX_LEN);

        /* Send the command with `write`, which is not buffered. */
        if (p->write_bytes_fn(fd, seq, written) == -1) {
            /* Can't recover... */
        }
    }
    return columns;

failed:
    return 80;
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void linenoise_make_beep_sound(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;

    char bip_screen_str[] = "\x7";
    (void)p->write_bytes_fn(instance->param.out_fd, bip_screen_str, sizeof(bip_screen_str));
}

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void free_completions(linenoise_completions_t *lc)
{
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    if (lc->cvec != NULL)
        free(lc->cvec);
}

/* This is an helper function for linenoise_edit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoise_state
 * structure as described in the structure definition. */
static int linenoise_complete_line(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    linenoise_completions_t lc = { 0, NULL };
    int nread, nwritten;
    char c = 0;
    int in_fd = instance->param.in_fd;

    p->completion_cb(l->buffer,&lc);
    if (lc.len == 0) {
        linenoise_make_beep_sound(instance);
    } else {
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                linenoise_instance_state_t saved = *l;

                l->len = l->cur_cursor_position = strlen(lc.cvec[i]);
                l->buffer = lc.cvec[i];
                linenoise_refresh_line(instance);
                l->len = saved.len;
                l->cur_cursor_position = saved.cur_cursor_position;
                l->buffer = saved.buffer;
            } else {
                linenoise_refresh_line(instance);
            }

            nread = p->read_bytes_fn(in_fd, &c, 1);
            if (nread <= 0) {
                free_completions(&lc);
                return -1;
            }

            switch(c) {
                case TAB: /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) linenoise_make_beep_sound(instance);
                    break;
                case ESC: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.len) linenoise_refresh_line(instance);
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.len) {
                        nwritten = snprintf(l->buffer,l->buffer_length,"%s",lc.cvec[i]);
                        l->len = l->cur_cursor_position = nwritten;
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
static int linenoise_edit_insert(linenoise_instance_t *instance, char c)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    int fd = instance->param.out_fd;
    if (l->len < l->buffer_length) {
        if (l->len == l->cur_cursor_position) {
            l->buffer[l->cur_cursor_position] = c;
            l->cur_cursor_position++;
            l->len++;
            l->buffer[l->len] = '\0';
            if ((!p->allow_multi_line && l->prompt_length+l->len < l->columns && !p->hints_cb)) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (p->write_bytes_fn(fd, &c,1) == -1) {
                    return -1;
                }
            } else {
                linenoise_refresh_line(instance);
            }
        } else {
            memmove(l->buffer+l->cur_cursor_position+1,l->buffer+l->cur_cursor_position,l->len-l->cur_cursor_position);
            l->buffer[l->cur_cursor_position] = c;
            l->len++;
            l->cur_cursor_position++;
            l->buffer[l->len] = '\0';
            linenoise_refresh_line(instance);
        }
    }
    return 0;
}

static int linenoise_insert_pasted_char(linenoise_instance_t *instance, char c)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;
    int fd = instance->param.out_fd;

    if (l->len < l->buffer_length && l->len == l->cur_cursor_position) {
        l->buffer[l->cur_cursor_position] = c;
        l->cur_cursor_position++;
        l->len++;
        l->buffer[l->len] = '\0';
        if (p->write_bytes_fn(fd, &c,1) == -1) {
            return -1;
        }
    }
    return 0;
}

/* Move cursor on the left. */
static void linenoise_edit_move_left(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;
    if (l->cur_cursor_position > 0) {
        l->cur_cursor_position--;
        linenoise_refresh_line(instance);
    }
}

/* Move cursor on the right. */
static void linenoise_edit_move_right(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;
    if (l->cur_cursor_position != l->len) {
        l->cur_cursor_position++;
        linenoise_refresh_line(instance);
    }
}

/* Move cursor to the start of the line. */
static void linenoise_edit_move_home(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;
    if (l->cur_cursor_position != 0) {
        l->cur_cursor_position = 0;
        linenoise_refresh_line(instance);
    }
}

/* Move cursor to the end of the line. */
static void linenoise_edit_move_end(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;
    if (l->cur_cursor_position != l->len) {
        l->cur_cursor_position = l->len;
        linenoise_refresh_line(instance);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
static void linenoise_edit_history_next(linenoise_instance_t *instance, int dir)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    if (l->history_length - l->history_index >= 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        char *buffer_copy = strdup(l->buffer);
        if (!buffer_copy) {
            return;
        }
        free(p->history[l->history_length - 1 - l->history_index]);
        p->history[l->history_length - 1 - l->history_index] = buffer_copy;
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= l->history_length){
            l->history_index = l->history_length-1;
            return;
        }
        strncpy(l->buffer, p->history[l->history_length - 1 - l->history_index], l->buffer_length);
        l->buffer[l->buffer_length-1] = '\0';
        l->len = l->cur_cursor_position = strlen(l->buffer);
        linenoise_refresh_line(instance);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
static void linenoise_edit_delete(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;

    if (l->len > 0 && l->cur_cursor_position < l->len) {
        memmove(l->buffer+l->cur_cursor_position,l->buffer+l->cur_cursor_position+1,l->len-l->cur_cursor_position-1);
        l->len--;
        l->buffer[l->len] = '\0';
        linenoise_refresh_line(instance);
    }
}

/* Backspace implementation. */
static void linenoise_edit_backspace(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;

    if (l->cur_cursor_position > 0 && l->len > 0){
        memmove(l->buffer + l->cur_cursor_position-1, l->buffer + l->cur_cursor_position, l->len - l->cur_cursor_position);
        l->cur_cursor_position--;
        l->len--;
        l->buffer[l->len] = '\0';
        linenoise_refresh_line(instance);
    }
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. */
static void linenoise_edit_delete_prev_word(linenoise_instance_t *instance)
{
    linenoise_instance_state_t *l = &instance->state;

    size_t old_pos = l->cur_cursor_position;
    size_t diff;

    while (l->cur_cursor_position > 0 && l->buffer[l->cur_cursor_position-1] == ' ')
        l->cur_cursor_position--;
    while (l->cur_cursor_position > 0 && l->buffer[l->cur_cursor_position-1] != ' ')
        l->cur_cursor_position--;
    diff = old_pos - l->cur_cursor_position;
    memmove(l->buffer+l->cur_cursor_position,l->buffer+old_pos,l->len-old_pos+1);
    l->len -= diff;
    linenoise_refresh_line(instance);
}

static uint32_t get_millis(void) {
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline size_t linenoise_prompt_len_ignore_escape_seq(const char *prompt)
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
static int linenoise_edit(linenoise_instance_t *instance, char *buffer, size_t buffer_length)
{
    linenoise_instance_param_t *p = &instance->param;
    linenoise_instance_state_t *l = &instance->state;

    uint32_t t1 = 0;
    int out_fd = instance->param.out_fd;
    int in_fd = instance->param.in_fd;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l->buffer = buffer;
    l->buffer_length = buffer_length;
    l->prompt_length = strlen(p->prompt);
    l->old_cursor_position = l->cur_cursor_position = 0;
    l->len = 0;
    l->columns = linenoise_get_columns(instance);
    l->max_rows_used = 0;
    l->history_index = 0;

    /* Buffer starts empty. */
    l->buffer[0] = '\0';
    l->buffer_length--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoise_history_add(instance, "");

    if (p->write_bytes_fn(out_fd, p->prompt, l->prompt_length) == -1) {
        return -1;
    }

    /* If the prompt has been registered with ANSI escape sequences
     * for terminal colors then we remove them from the prompt length
     * calculation. */
    l->prompt_length = linenoise_prompt_len_ignore_escape_seq(p->prompt);

    while(1) {
        char c;

        /**
         * To determine whether the user is pasting data or typing itself, we
         * need to calculate how many milliseconds elapsed between two key
         * presses. Indeed, if there is less than LINENOISE_PASTE_KEY_DELAY
         * (typically 30-40ms), then a paste is being performed, else, the
         * user is typing.
         * NOTE: pressing a key down without releasing it will also spend
         * about 40ms (or even more)
         */
        t1 = get_millis();
        int nread = p->read_bytes_fn(in_fd, &c, 1);
        if (nread <= 0) {
            return l->len;
        }

        if ( (get_millis() - t1) < LINENOISE_PASTE_KEY_DELAY && c != ENTER) {
            /* Pasting data, insert characters without formatting.
             * This can only be performed when the cursor is at the end of the
             * line. */
            if (linenoise_insert_pasted_char(instance, c)) {
                return -1;
            }
            continue;
        }

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && p->completion_cb != NULL) {
            int c2 = linenoise_complete_line(instance);
            /* Return on errors */
            if (c2 < 0) return l->len;
            /* Read next character when 0 */
            if (c2 == 0) continue;
            c = c2;
        }

        switch(c) {
        case ENTER:    /* enter */
            l->history_length--;
            free(p->history[l->history_length]);
            if (p->allow_multi_line) {
                linenoise_edit_move_end(instance);
            }
            if (p->hints_cb) {
                /* Force a refresh without hints to leave the previous
                 * line as the user typed it after a newline. */
                linenoiseHintsCallback *hc = p->hints_cb;
                p->hints_cb = NULL;
                linenoise_refresh_line(instance);
                p->hints_cb = hc;
            }
            return (int)l->len;
        case CTRL_C:     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case BACKSPACE:   /* backspace */
        case CTRL_H:     /* ctrl-h */
            linenoise_edit_backspace(instance);
            break;
        case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
            if (l->len > 0) {
                linenoise_edit_delete(instance);
            } else {
                l->history_length--;
                free(p->history[l->history_length]);
                return -1;
            }
            break;
        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (l->cur_cursor_position > 0 && l->cur_cursor_position < l->len) {
                int aux = l->buffer[l->cur_cursor_position-1];
                l->buffer[l->cur_cursor_position-1] = l->buffer[l->cur_cursor_position];
                l->buffer[l->cur_cursor_position] = aux;
                if (l->cur_cursor_position != l->len-1) l->cur_cursor_position++;
                linenoise_refresh_line(instance);
            }
            break;
        case CTRL_B:     /* ctrl-b */
            linenoise_edit_move_left(instance);
            break;
        case CTRL_F:     /* ctrl-f */
            linenoise_edit_move_right(instance);
            break;
        case CTRL_P:    /* ctrl-p */
            linenoise_edit_history_next(instance, LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N:    /* ctrl-n */
            linenoise_edit_history_next(instance, LINENOISE_HISTORY_NEXT);
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            l->buffer[0] = '\0';
            l->cur_cursor_position = l->len = 0;
            linenoise_refresh_line(instance);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            l->buffer[l->cur_cursor_position] = '\0';
            l->len = l->cur_cursor_position;
            linenoise_refresh_line(instance);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            linenoise_edit_move_home(instance);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            linenoise_edit_move_end(instance);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            linenoise_clear_screen(instance);
            linenoise_refresh_line(instance);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            linenoise_edit_delete_prev_word(instance);
            break;
        case ESC: {     /* escape sequence */
            /* ESC [ sequences. */
            char seq[3];
            int r = p->read_bytes_fn(in_fd, seq, 2);
            if (r != 2) {
                return -1;
            }
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    r = p->read_bytes_fn(in_fd, seq + 2, 1);
                    if (r != 1) {
                        return -1;
                    }
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': /* Delete key. */
                            linenoise_edit_delete(instance);
                            break;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': /* Up */
                        linenoise_edit_history_next(instance, LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        linenoise_edit_history_next(instance, LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        linenoise_edit_move_right(instance);
                        break;
                    case 'D': /* Left */
                        linenoise_edit_move_left(instance);
                        break;
                    case 'H': /* Home */
                        linenoise_edit_move_home(instance);
                        break;
                    case 'F': /* End*/
                        linenoise_edit_move_end(instance);
                        break;
                    }
                }
            }
            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': /* Home */
                    linenoise_edit_move_home(instance);
                    break;
                case 'F': /* End*/
                    linenoise_edit_move_end(instance);
                    break;
                }
            }
            break;
        }
        default:
            if (linenoise_edit_insert(instance,c)) return -1;
            break;
        }
        fsync(instance->param.out_fd);
    }
    return l->len;
}

static int linenoise_raw(linenoise_instance_t *instance, char *buffer, size_t buffer_length) {
    int count;

    if (buffer_length == 0) {
        errno = EINVAL;
        return -1;
    }

    count = linenoise_edit(instance, buffer, buffer_length);
    instance->param.write_bytes_fn(instance->param.out_fd, "\n", 1);
    return count;
}

static int linenoise_dumb(linenoise_instance_t *instance, char* buffer, size_t buffer_length) {
    linenoise_instance_param_t *p = &instance->param;

    p->write_bytes_fn(instance->param.out_fd, p->prompt, sizeof(p->prompt));

    size_t count = 0;
    const int in_fd = instance->param.in_fd;
    char c = 'c';

    while (count < buffer_length) {

        int nread = p->read_bytes_fn(in_fd, &c, 1);
        if (nread < 0) {
            return nread;
        }
        if (c == '\n') {
            break;
        } else if (c == BACKSPACE || c == CTRL_H) {
            if (count > 0) {
                buffer[count - 1] = 0;
                count--;

                /* Only erase symbol echoed from in_fd. */
                char erase_symbol_str[] = "\x08";
                p->write_bytes_fn(instance->param.out_fd, erase_symbol_str, sizeof(erase_symbol_str)); /* Windows CMD: erase symbol under cursor */
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
        p->write_bytes_fn(instance->param.out_fd, &c, 1); /* echo */
    }
    p->write_bytes_fn(instance->param.out_fd, "\n", 1);
    return count;
}

static void linenoise_sanitize(char* src) {
    char* dst = src;
    for (int c = *src; c != 0; src++, c = *src) {
        if (isprint(c)) {
            *dst = c;
            ++dst;
        }
    }
    *dst = 0;
}

/* TODO[linenoise]: this should be removed once we update the console component.
 * this function was originally here in case repl_stop was used, to allow the user
 * to provide a strong definition of this function in order to set e.g., the read
 * mode and the read functions */
void linenoise_set_read_characteristics(linenoise_instance_t *instance)
{
    /* By default linenoise uses blocking reads */
    int fd_in = instance->param.in_fd;
    int flags = fcntl(fd_in, F_GETFL);
    flags &= ~O_NONBLOCK;
    (void)fcntl(fd_in, F_SETFL, flags);

    instance->param.read_bytes_fn = read;
}

static int linenoise_probe(linenoise_instance_t *instance)
{
    linenoise_instance_param_t *p = &instance->param;

    linenoise_set_read_characteristics(instance);

    /* Make sure we are in non blocking mode before performing the terminal probing */
    int fd_in = instance->param.in_fd;
    int old_flags = fcntl(fd_in, F_GETFL);
    int new_flags = old_flags | O_NONBLOCK;
    int res = fcntl(fd_in, F_SETFL, new_flags);
    if (res != 0) {
        return -1;
    }

    /* Device status request */
    char status_request_str[] = "\x1b[5n";
    p->write_bytes_fn(instance->param.out_fd, status_request_str, sizeof(status_request_str));

    /* Try to read response */
    int timeout_ms = 500;
    const int retry_ms = 10;
    size_t read_bytes = 0;
    while (timeout_ms > 0 && read_bytes < 4) { // response is ESC[0n or ESC[3n
        usleep(retry_ms * 1000);
        timeout_ms -= retry_ms;
        char c;
        int cb = read(fd_in, &c, 1);
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

/*-------------------------------------------------------------------------------------------------------
 * DEPRECATED LINENOISE API
 *-------------------------------------------------------------------------------------------------------*/

__attribute__((weak)) void linenoiseSetReadCharacteristics(void)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_set_read_characteristics(instance);
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str)
    __attribute__((alias("linenoise_add_completion")));

void linenoiseSetMultiLine(int ml)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_set_multi_line(instance, (bool)ml);
}

void linenoiseSetDumbMode(int set)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_set_dumb_mode(instance, (bool)set);
}

bool linenoiseIsDumbMode(void)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    return linenoise_is_dumb_mode(instance);
}

void linenoiseAllowEmpty(bool val)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_set_empty_line(instance, (bool)val);
}

void linenoiseSetWriteFunction(linenoise_write_bytes_fn write_fn)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    instance->param.write_bytes_fn = write_fn;
}

void linenoiseSetReadFunction(linenoise_read_bytes_fn read_fn)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    instance->param.read_bytes_fn = read_fn;
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    instance->param.completion_cb = fn;
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void linenoiseSetHintsCallback(linenoiseHintsCallback *fn)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    instance->param.hints_cb = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoiseSetHintsCallback(). */
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *fn)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    instance->param.free_hints_cb = fn;
}

/* Clear the screen. Used to handle ctrl+l */
void linenoiseClearScreen(void)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_clear_screen(instance);
}

int linenoiseProbe(void)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    return linenoise_probe(instance);
}

/* The high level function that is the main API of the linenoise library. */
char *linenoise(const char *prompt)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();

    /* update the default prompt value set when the static linenoise
     * instance was created */
    const char *prompt_copy = instance->param.prompt;
    instance->param.prompt = prompt;

    char *cmd_line = linenoise_loop(instance);

    /* reset the prompt to its default value */
    instance->param.prompt = prompt_copy;

    /* return the line */
    return cmd_line;
}

/* This is just a wrapper the user may want to call in order to make sure
 * the linenoise returned buffer is freed with the same allocator it was
 * created with. Useful when the main program is using an alternative
 * allocator. */
void linenoiseFree(void *ptr)
    __attribute__((alias("linenoise_free")));

void linenoiseHistoryFree(void)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_history_free(instance);
}

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    const bool ret_val = linenoise_history_add(instance, line);
    if (!ret_val) {
        return -1;
    }
    return 0;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int linenoiseHistorySetMaxLen(int len)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    const bool ret_val = linenoise_history_set_max_len(instance, len);
    if (!ret_val) {
        return 0;
    }
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char *filename)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    const bool ret_val = linenoise_history_save(instance, filename);
    if (!ret_val) {
        return -1;
    }
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const char *filename)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    const bool ret_val = linenoise_history_load(instance, filename);
    if (!ret_val) {
        return -1;
    }
    return 0;
}

/* Set line maximum length. If len parameter is smaller than
 * LINENOISE_MINIMAL_MAX_LINE, -1 is returned
 * otherwise 0 is returned. */
int linenoiseSetMaxLineLen(size_t len)
{
    linenoise_instance_t *instance = linenoise_get_static_instance();
    linenoise_set_max_cmd_line_length(instance, len);
    if (linenoise_get_max_cmd_line_length(instance) != len) {
        return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------------------------------------------
 * NEW LINENOISE API
 *-------------------------------------------------------------------------------------------------------*/

#define LINENOISE_CHECK_INSTANCE(handle) assert((linenoise_instance_t*)handle->self == (linenoise_instance_t*)handle)

linenoise_instance_param_t linenoise_get_instance_param_default(void)
{
    return (linenoise_instance_param_t) {
        .prompt = LINENOISE_DEFAULT_PROMPT,
        .max_cmd_line_length = LINENOISE_DEFAULT_MAX_LINE,
        .history_max_length = LINENOISE_DEFAULT_HISTORY_MAX_LENGTH,
        .allow_multi_line = false,  /* Multi line mode. Default is single line. */
        .allow_empty_line = true, /* Allow linenoise to return an empty string. On by default */
        .allow_dumb_mode = false, /* Dumb mode where line editing is disabled. Off by default */
        .completion_cb = NULL,
        .hints_cb = NULL,
        .free_hints_cb = NULL,
        .write_bytes_fn = linenoise_default_write_bytes,
        .read_bytes_fn = read,
        .history = NULL,
    };
}

linenoise_handle_t linenoise_create_instance(linenoise_instance_param_t *param)
{
    if (!param) {
        return NULL;
    }

    linenoise_instance_t *instance = malloc(sizeof(linenoise_instance_t));
    assert(instance != NULL);

    /* make sure the history is NULL since the linenoise library will allocate it */
    if (param->history != NULL) {
        return NULL;
    }

    if (!param->prompt) {
        param->prompt = LINENOISE_DEFAULT_PROMPT;
    }
    if (!param->max_cmd_line_length) {
        param->max_cmd_line_length = LINENOISE_DEFAULT_MAX_LINE;
    }
    if (!param->history_max_length) {
        param->history_max_length = LINENOISE_DEFAULT_HISTORY_MAX_LENGTH;
    }
    if (param->read_bytes_fn == NULL) {
        param->read_bytes_fn = read;
    }
    if (param->write_bytes_fn == NULL) {
        param->write_bytes_fn = linenoise_default_write_bytes;
    }

    instance->param = *param;

    const int probe_status = linenoise_probe(instance);
    if (probe_status == 0) {
        /* escape sequences supported*/
        instance->param.allow_dumb_mode = false;
    } else {
        /* error during the probing or escape sequences not supported */
        instance->param.allow_dumb_mode = true;
    }

    /* set the state part of the linenoise_instance_t to 0 to init all values to 0 (or NULL) */
    memset(&instance->state, 0x00, sizeof(linenoise_instance_state_t));

    /* set the self value to the handle of instance */
    instance->self = instance;

    return (linenoise_handle_t)instance;
}

void linenoise_delete_instance(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_t *instance = (linenoise_instance_t*)handle;

    // free the history first since it was allocated by linenoise
    linenoise_history_free(handle);

    // reset the memory
    memset(instance, 0x00, sizeof(linenoise_instance_t));

    // free the instance
    free(instance);
}

char* linenoise_loop(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_t *instance = (linenoise_instance_t*)handle;
    linenoise_instance_param_t *p = &instance->param;

    char *cmd_line_buffer = calloc(1, p->max_cmd_line_length);
    int count = 0;
    if (cmd_line_buffer == NULL) {
        return NULL;
    }
    if (!p->allow_dumb_mode) {
        count = linenoise_raw(instance, cmd_line_buffer, p->max_cmd_line_length);
    } else {
        count = linenoise_dumb(instance, cmd_line_buffer, p->max_cmd_line_length);
    }
    if (count > 0) {
        linenoise_sanitize(cmd_line_buffer);
        count = strlen(cmd_line_buffer);
    } else if (count == 0 && p->allow_empty_line) {
        /* will return an empty (0-length) string */
    } else {
        free(cmd_line_buffer);
        return NULL;
    }
    return cmd_line_buffer;
}

void linenoise_free(void* ptr)
{
    free(ptr);
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void linenoise_add_completion(linenoise_completions_t *lc, const char *str)
{
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}

bool linenoise_history_add(linenoise_handle_t handle, const char *line)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_param_t *p = &((linenoise_instance_t*)handle)->param;
    linenoise_instance_state_t *l = &((linenoise_instance_t*)handle)->state;

    if (p->history_max_length == 0) {
        return false;
    }

    /* Initialization on first call. */
    if (p->history == NULL) {
        p->history = malloc(sizeof(char*) * p->history_max_length);
        if (p->history == NULL) {
            return false;
        }
        memset(p->history, 0, (sizeof(char*) * p->history_max_length));

        l->history_length = 0;
    }

    /* Don't add duplicated lines. */
    if (l->history_length && !strcmp(p->history[l->history_length - 1], line)) {
        return false;
    }

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
    char *line_copy = strdup(line);
    if (!line_copy) {
        return false;
    }
    if (l->history_length == p->history_max_length) {
        free(p->history[0]);
        memmove(p->history, p->history + 1, sizeof(char*) * (p->history_max_length-1));
        l->history_length--;
    }
    p->history[l->history_length] = line_copy;
    l->history_length++;
    return true;
}

bool linenoise_history_save(linenoise_handle_t handle, const char *filename)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_t *instance = (linenoise_instance_t*)handle;

    FILE *fp;
    int j;

    fp = fopen(filename, "w");
    if (fp == NULL) {
        return false;
    }

    for (j = 0; j < instance->state.history_length; j++) {
        fprintf(fp, "%s\n", instance->param.history[j]);
    }

    fclose(fp);
    return true;
}

bool linenoise_history_load(linenoise_handle_t handle, const char *filename)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_t *instance = (linenoise_instance_t*)handle;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;
    }

    char *buf = calloc(1, instance->param.max_cmd_line_length);
    if (buf == NULL) {
        fclose(fp);
        return false;
    }

    while (fgets(buf, instance->param.max_cmd_line_length, fp) != NULL) {
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoise_history_add(handle, buf);
    }

    free(buf);
    fclose(fp);

    return true;
}

bool linenoise_history_set_max_len(linenoise_handle_t handle, int new_length)
{
    LINENOISE_CHECK_INSTANCE(handle);

    linenoise_instance_param_t *p = &((linenoise_instance_t*)handle)->param;
    linenoise_instance_state_t *l = &((linenoise_instance_t*)handle)->state;

    if (new_length == p->history_max_length) {
        /* the requested history size is the same as the current one, return true
         * without changing anything */
        return true;
    }
    if (new_length < 1) {
        return false;
    }

    char **new_history;
    if (p->history) {
        int to_copy = l->history_length;

        new_history = malloc(sizeof(char*) * new_length);
        if (new_history == NULL) {
            return false;
        }

        /* If we can't copy everything, free the elements we'll not use. */
        if (new_length < to_copy) {
            int j;

            for (j = 0; j < to_copy - new_length; j++) {
                free(p->history[j]);
            }
            to_copy = new_length;
        }
        memset(new_history, 0, sizeof(char*) * new_length);
        memcpy(new_history, p->history + (l->history_length - to_copy), sizeof(char*)*to_copy);
        free(p->history);
        p->history = new_history;
    }

    p->history_max_length = new_length;

    if (l->history_length > p->history_max_length) {
        l->history_length = p->history_max_length;
    }
    return true;
}

void linenoise_history_free(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    linenoise_instance_t *instance = (linenoise_instance_t*)handle;

    if (instance->param.history) {
        for (int j = 0; j < instance->state.history_length; j++) {
            free(instance->param.history[j]);
        }
        free(instance->param.history);
    }
    instance->param.history = NULL;
    instance->state.history_length = 0;
}

void linenoise_clear_screen(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    linenoise_instance_t *instance = (linenoise_instance_t*)handle;

    char erase_screen_str[] = "\x1b[H\x1b[2J";
    (void)instance->param.write_bytes_fn(instance->param.out_fd, erase_screen_str, sizeof(erase_screen_str));
}

void linenoise_set_empty_line(linenoise_handle_t handle, bool empty_line)
{
    LINENOISE_CHECK_INSTANCE(handle);
    ((linenoise_instance_t*)handle)->param.allow_empty_line = empty_line;
}

bool linenoise_is_empty_line(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    return ((linenoise_instance_t*)handle)->param.allow_empty_line;
}

void linenoise_set_multi_line(linenoise_handle_t handle, bool multi_line)
{
    LINENOISE_CHECK_INSTANCE(handle);
    ((linenoise_instance_t*)handle)->param.allow_multi_line = multi_line;
}

bool linenoise_is_multi_line(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    return ((linenoise_instance_t*)handle)->param.allow_multi_line;
}

void linenoise_set_dumb_mode(linenoise_handle_t handle, bool dumb_mode)
{
    LINENOISE_CHECK_INSTANCE(handle);
    ((linenoise_instance_t*)handle)->param.allow_dumb_mode = dumb_mode;
}

bool linenoise_is_dumb_mode(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    return ((linenoise_instance_t*)handle)->param.allow_dumb_mode;
}

void linenoise_set_max_cmd_line_length(linenoise_handle_t handle, size_t length)
{
    LINENOISE_CHECK_INSTANCE(handle);
    if (length >= LINENOISE_MINIMAL_MAX_LINE) {
        ((linenoise_instance_t*)handle)->param.max_cmd_line_length = length;
    }
}

size_t linenoise_get_max_cmd_line_length(linenoise_handle_t handle)
{
    LINENOISE_CHECK_INSTANCE(handle);
    return ((linenoise_instance_t*)handle)->param.max_cmd_line_length;
}
