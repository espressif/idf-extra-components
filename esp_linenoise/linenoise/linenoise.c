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

#include <fcntl.h>
#include "esp_linenoise_private.h"
#include "esp_linenoise.h"
#include "linenoise.h"
#include "esp_err.h"

static esp_linenoise_instance_t *s_linenoise_instance = NULL;
static linenoiseCompletionCallback *s_completion_cb = NULL;

static void completion_default_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    /* unused because incompatible with legacy code */
    (void)cb;

    if (!s_completion_cb) {
        return;
    }
    s_completion_cb(str, (linenoiseCompletions *)cb_ctx);
}

static inline __attribute__((always_inline))
esp_linenoise_instance_t *linenoise_get_static_instance(void)
{
    if (!s_linenoise_instance) {
        s_linenoise_instance = esp_linenoise_create_instance_static();
    }
    return s_linenoise_instance;
}

__attribute__((weak)) void linenoiseSetReadCharacteristics(void)
{
    linenoiseSetReadFunction(read);

    /* By default linenoise uses blocking reads */
    int fd_in = s_linenoise_instance->config.in_fd;
    int flags = fcntl(fd_in, F_GETFL);
    flags &= ~O_NONBLOCK;
    (void)fcntl(fd_in, F_SETFL, flags);
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str)
{
    esp_linenoise_add_completion(lc, str);
}

void linenoiseSetMultiLine(int ml)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    /* since we get the static instance, the input validation will always
     * yield a positive result, it is not necessary to check the return value */
    (void)esp_linenoise_set_multi_line(instance, (bool)ml);
}

void linenoiseSetDumbMode(int set)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    /* since we get the static instance, the input validation will always
     * yield a positive result, it is not necessary to check the return value */
    (void)esp_linenoise_set_dumb_mode(instance, (bool)set);
}

bool linenoiseIsDumbMode(void)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    bool is_dump_mode = false;
    /* since we get the static instance, the input validation will always
     * yield a positive result, it is not necessary to check the return value */
    (void)esp_linenoise_is_dumb_mode(instance, &is_dump_mode);
    return is_dump_mode;
}

void linenoiseAllowEmpty(bool val)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    /* since we get the static instance, the input validation will always
     * yield a positive result, it is not necessary to check the return value */
    (void)esp_linenoise_set_empty_line(instance, (bool)val);
}

void linenoiseSetWriteFunction(linenoiseWriteBytesFn write_fn)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    if (write_fn != NULL) {
        instance->config.write_bytes_cb = write_fn;
    }
}

void linenoiseSetReadFunction(linenoiseReadBytesFn read_fn)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    if (read_fn != NULL) {
        instance->config.read_bytes_cb = read_fn;
    }
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    if (fn != NULL) {
        s_completion_cb = fn;
        instance->config.completion_cb = completion_default_cb;
    }
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void linenoiseSetHintsCallback(linenoiseHintsCallback *fn)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    if (fn != NULL) {
        instance->config.hints_cb = fn;
    }
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoiseSetHintsCallback(). */
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *fn)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    if (fn != NULL) {
        instance->config.free_hints_cb = fn;
    }
}

/* Clear the screen. Used to handle ctrl+l */
void linenoiseClearScreen(void)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    (void)esp_linenoise_clear_screen(instance);
}

int linenoiseProbe(void)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();

    linenoiseSetReadCharacteristics();

    return esp_linenoise_probe(instance);
}

/* The high level function that is the main API of the linenoise library. */
char *linenoise(const char *prompt)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();

    /* update the default prompt value set when the static linenoise
     * instance was created */
    const char *prompt_copy = instance->config.prompt;
    instance->config.prompt = prompt;

    size_t cmd_line_length = 0;
    esp_err_t ret_val = esp_linenoise_get_max_cmd_line_length(instance, &cmd_line_length);
    if (ret_val != ESP_OK) {
        cmd_line_length = ESP_LINENOISE_COMMAND_MAX_LEN;
    }
    char *cmd_line = calloc(1, cmd_line_length);
    if (!cmd_line) {
        return NULL;
    }

    ret_val = esp_linenoise_get_line(instance, cmd_line, cmd_line_length);
    if (ret_val != ESP_OK) {
        free(cmd_line);
        return NULL;
    }

    /* reset the prompt to its default value */
    instance->config.prompt = prompt_copy;

    /* return the line */
    return cmd_line;
}

/* This is just a wrapper the user may want to call in order to make sure
 * the linenoise returned buffer is freed with the same allocator it was
 * created with. Useful when the main program is using an alternative
 * allocator. */
void linenoiseFree(void *ptr)
{
    free(ptr);
}

void linenoiseHistoryFree(void)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    (void)esp_linenoise_history_free(instance);
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
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    const esp_err_t ret_val = esp_linenoise_history_add(instance, line);
    if (ret_val != ESP_OK) {
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
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    const esp_err_t ret_val = esp_linenoise_history_set_max_len(instance, len);
    if (ret_val != ESP_OK) {
        return 0;
    }
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char *filename)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    const esp_err_t ret_val = esp_linenoise_history_save(instance, filename);
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
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    const esp_err_t ret_val = esp_linenoise_history_load(instance, filename);
    if (ret_val != ESP_OK) {
        return -1;
    }
    return 0;
}

/* Set line maximum length. If len configeter is smaller than
 * ESP_LINENOISE_MINIMAL_MAX_LINE, -1 is returned
 * otherwise 0 is returned. */
int linenoiseSetMaxLineLen(size_t len)
{
    esp_linenoise_instance_t *instance = linenoise_get_static_instance();
    esp_err_t ret_val = esp_linenoise_set_max_cmd_line_length(instance, len);
    if (ret_val != ESP_OK) {
        return -1;
    }
    return 0;
}
