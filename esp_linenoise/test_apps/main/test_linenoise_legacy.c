/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "fcntl.h"
#include "unity.h"
#include "linenoise/linenoise.h"
#include "test_utils.h"

static int s_socket_fd[2];
static int s_original_stdin_fd = -1;
static int s_original_stdout_fd = -1;
static char *s_returned_line = NULL;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static bool s_completions_called = false;
static bool s_hint_called = false;
static bool s_free_hint_called = false;

void custom_legacy_completion_cb(const char *str, linenoiseCompletions *lc)
{
    // we just want to see that the callback is indeed called
    // so flip the s_completions_called to true
    if (!s_completions_called) {
        s_completions_called = true;
    }
}

char *custom_legacy_hint_cb(const char *str, int *color, int *bold)
{
    // we just want to see that the callback is indeed called
    // so flip the s_hint_called to true
    if (!s_hint_called) {
        s_hint_called = true;
    }

    return "something";
}

void custom_legacy_free_hint_cb(void *ptr)
{
    // we just want to see that the callback is indeed called
    // so flip the s_free_hint_called to true
    if (!s_free_hint_called) {
        s_free_hint_called = true;
    }
}

ssize_t custom_legacy_write(int fd, const void *buf, size_t count)
{
    // printf("writing : ");
    // for (size_t i = 0; i < count; i++) {
    //     printf("(%x, %c) ", *((char*)buf + i), *((char*)buf + i));
    // }
    // printf("\n");

    // find the request in the list of commands and send the response
    for (size_t i = 0; i < commands_count; i++) {
        if (strstr(commands[i].request, buf) != NULL) {
            const char *response = commands[i].response;
            if (response != NULL) {
                const size_t size = strlen(commands[i].response);

                // write the expected response to the socket, so linenoise
                // can read the response
                const ssize_t nwrite = write(s_socket_fd[1], response, size);
                TEST_ASSERT_EQUAL(size, nwrite);
            }

            // return the count like a normal write would do
            // do not propagate to the socket to not pollute
            // the buffers
            return count;
        }
    }

    // otherwise just propagate the write
    return write(fd, buf, count);
}

static void test_setup(int socket_fd[2], pthread_mutex_t *s_lock)
{
    // 2 fd are generated, simulating the full-duplex
    // communication between linenoise and the terminal
    socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fd);

    // redirect stdin and stdout since legacy linenoise uses
    // stdin and stdout only as default streams
    s_original_stdin_fd = dup(STDIN_FILENO);
    s_original_stdout_fd = dup(STDOUT_FILENO);
    TEST_ASSERT(dup2(s_socket_fd[0], STDIN_FILENO) >= 0);
    TEST_ASSERT(dup2(s_socket_fd[0], STDOUT_FILENO) >= 0);
    close(s_socket_fd[0]);

    // assure that the read will be blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);

    linenoiseSetCompletionCallback(custom_legacy_completion_cb);
    linenoiseSetHintsCallback(custom_legacy_hint_cb);
    linenoiseSetFreeHintsCallback(custom_legacy_free_hint_cb);
    /* don't set the custom read, since we don't need it. also it tests that the default
     * read is used by linenoise */
    linenoiseSetWriteFunction(custom_legacy_write);

    const int is_dumb_mode = linenoiseProbe();
    if (is_dumb_mode) {
        printf("running dumb mode\n");
        linenoiseSetDumbMode(1);
    } else {
        printf("running normal mode\n");
        linenoiseSetDumbMode(0);
    }

    // take the semaphore
    pthread_mutex_lock(s_lock);
}

static void test_teardown(int socket_fd[2])
{
    memset(s_returned_line, 0, CMD_LINE_LENGTH);

    // Restore the default streams
    dup2(s_original_stdin_fd, STDIN_FILENO);
    dup2(s_original_stdout_fd, STDOUT_FILENO);
    close(s_original_stdin_fd);
    close(s_original_stdout_fd);

    close(socket_fd[0]);
    close(socket_fd[1]);

    // unlock the mutex for the next test
    pthread_mutex_unlock(&s_lock);
}

static void *get_line_task(void *arg)
{
    char *prompt = (char *)arg;

    // wait for the instance to properly initialize before unlocking
    // the mutex so the test can run
    usleep(100000);

    // release the mutex so the test can start sending data
    pthread_mutex_unlock(&s_lock);

    s_returned_line = linenoise(prompt);
    return NULL;
}

TEST_CASE("legacy linenoise() returns line read from stdin and writes to stdout", "[linenoise]")
{
    test_setup(s_socket_fd, &s_lock);

    pthread_t thread_id;
    char *prompt = ">>>";
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, prompt));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&s_lock);

    const char *input_line = "unit test input";
    test_send_characters(s_socket_fd[1], input_line);

    // Write newline to trigger prompt output + return from loop
    test_send_characters(s_socket_fd[1], "\n");

    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    /* verify that the string was processed properly by linenoise() */
    TEST_ASSERT_NOT_NULL(s_returned_line);
    TEST_ASSERT_NOT_NULL(strstr(input_line, s_returned_line));

    // Verify prompt string is found in output
    char full_cmd_line[32] = {0};
    const ssize_t nread = read(s_socket_fd[1], full_cmd_line, 32);
    TEST_ASSERT_NOT_EQUAL(-1, nread);

    TEST_ASSERT_NOT_NULL(strstr(full_cmd_line, prompt));

    test_teardown(s_socket_fd);
}

TEST_CASE("legacy check completion, hint and free hint callback", "[linenoise]")
{
    test_setup(s_socket_fd, &s_lock);

    pthread_t thread_id;
    char *prompt = ">>>";
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, prompt));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&s_lock);

    // Insert "word_a" this should trigger the hint cb and free hints cb
    test_send_characters(s_socket_fd[1], "word_a");

    // TAB: this should trigger the completions cb
    test_send_characters(s_socket_fd[1], COMPOUND_LITERAL(TAB));

    // Newline (accept)
    test_send_characters(s_socket_fd[1], "\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_EQUAL(true, s_hint_called);
    TEST_ASSERT_EQUAL(true, s_completions_called);
    TEST_ASSERT_EQUAL(true, s_free_hint_called);

    test_teardown(s_socket_fd);
}
