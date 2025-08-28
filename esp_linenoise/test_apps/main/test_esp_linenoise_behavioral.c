/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fcntl.h"
#include "unity.h"
#include "esp_linenoise.h"
#include "test_utils.h"

static int s_socket_fd_a[2];
static int s_socket_fd_b[2];
static esp_linenoise_handle_t s_linenoise_hdl;
static char s_line_returned[CMD_LINE_LENGTH] = {0};

static bool s_completions_called = false;
static bool s_hint_called = false;
static bool s_free_hint_called = false;

static void custom_completion_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    // we just want to see that the callback is indeed called
    // so flip the s_completions_called to true
    if (!s_completions_called) {
        s_completions_called = true;
    }
}

static char *custom_hint_cb(const char *str, int *color, int *bold)
{
    // we just want to see that the callback is indeed called
    // so flip the s_hint_called to true
    if (!s_hint_called) {
        s_hint_called = true;
    }

    return "something";
}

static void custom_free_hint_cb(void *ptr)
{
    // we just want to see that the callback is indeed called
    // so flip the s_free_hint_called to true
    if (!s_free_hint_called) {
        s_free_hint_called = true;
    }
}

static ssize_t custom_read(int fd, void *buf, size_t count)
{
    int nread = -1;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    int ret = select(fd + 1, &rfds, NULL, NULL, NULL);
    if (ret > 0 && FD_ISSET(fd, &rfds)) {
        nread = read(fd, buf, count);
    }

    return nread;
}

static ssize_t custom_write(int fd, const void *buf, size_t count)
{
    // find the request in the list of commands and send the response
    for (size_t i = 0; i < commands_count; i++) {
        if (strstr(commands[i].request, buf) != NULL) {
            const char *response = commands[i].response;
            if (response != NULL) {
                const size_t size = strlen(commands[i].response);

                // write the expected response to the socket, so linenoise
                // can read the response
                // conveniently, the socketpair FDs are following each other so
                // to simulate a write from the device, call write on fd + 1
                const ssize_t nwrite = write(fd + 1, response, size);
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

static void test_instance_setup(int socket_fd[2], pthread_mutex_t *lock, esp_linenoise_config_t *config)
{
    // 2 fd are generated, simulating the full-duplex
    // communication between linenoise and the terminal
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fd));

    // assure that the read will be blocking
    int flags = fcntl(socket_fd[0], F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(socket_fd[0], F_SETFL, flags);

    flags = fcntl(socket_fd[1], F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(socket_fd[0], F_SETFL, flags);

    esp_linenoise_get_instance_config_default(config);

    // all data written by the test on socket_fd[1] will
    // bbe available for reading on socket_fd[0]. All data
    // written to socket_fd[0] will be available for reading
    // on socket_fd[1].
    config->in_fd = socket_fd[0];
    config->out_fd = socket_fd[0];

    // redirect read and write calls from linenoise to be able
    // to e.g., process sequences sent from linenoise and thus
    // simulate that the terminal supports escape sequences
    config->read_bytes_cb = custom_read;
    config->write_bytes_cb = custom_write;

    // take the semaphore
    pthread_mutex_lock(lock);
}

static void test_instance_teardown(int socket_fd[2], esp_linenoise_handle_t handle, pthread_mutex_t *lock)
{
    memset(s_line_returned, 0, CMD_LINE_LENGTH);

    esp_linenoise_delete_instance(handle);
    close(socket_fd[0]);
    close(socket_fd[1]);

    // unlock the mutex for the next test
    pthread_mutex_destroy(lock);
}

typedef struct get_line_args {
    pthread_mutex_t *lock;
    TaskHandle_t parent_task;
    esp_linenoise_config_t *config;
} get_line_args_t;

static void get_line_task(void *args)
{
    get_line_args_t *task_args = (get_line_args_t *)args;

    s_linenoise_hdl = esp_linenoise_create_instance(task_args->config);
    TEST_ASSERT_NOT_NULL(s_linenoise_hdl);

    // wait for the instance to properly initialize before unlocking
    // the mutex so the test can run
    usleep(100000);

    // release the mutex so the test can start sending data
    pthread_mutex_unlock(task_args->lock);

    esp_err_t ret_val = esp_linenoise_get_line(s_linenoise_hdl, s_line_returned, CMD_LINE_LENGTH);
    TEST_ASSERT_EQUAL(ESP_OK, ret_val);

    xTaskNotifyGive(task_args->parent_task);

    vTaskDelete(NULL);
}

typedef struct get_line_task_args {
    esp_linenoise_handle_t handle;
    TaskHandle_t parent_task;
    pthread_mutex_t *lock;
    esp_err_t ret_val;
    char *buf;
    size_t buf_size;
} get_line_task_args_t;

static void get_line_task_w_args(void *args)
{
    get_line_task_args_t *task_args = (get_line_task_args_t *)args;

    // wait for the instance to properly initialize before unlocking
    // the mutex so the test can run
    usleep(100000);

    // release the mutex so the test can start sending data
    pthread_mutex_unlock(task_args->lock);

    task_args->ret_val = esp_linenoise_get_line(task_args->handle, task_args->buf, task_args->buf_size);

    xTaskNotifyGive(task_args->parent_task);
    vTaskDelete(NULL);
}

TEST_CASE("esp_linenoise_get_line() returns line read from in_fd", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    const char *input_line = "unit test input";
    test_send_characters(s_socket_fd_a[1], input_line);

    // Write newline to trigger prompt output + return from loop
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING(input_line, s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("custom prompt string appears on output", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    const char *custom_prompt = ">>> ";
    config.prompt = (char *)custom_prompt;  // cast away const as config expects char*

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Write newline to trigger prompt output + return from loop
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Verify prompt string is found in output
    char full_cmd_line[32] = {0};
    const ssize_t nread = read(s_socket_fd_a[1], full_cmd_line, 32);
    TEST_ASSERT_NOT_EQUAL(-1, nread);

    TEST_ASSERT_NOT_NULL(strstr(full_cmd_line, custom_prompt));

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("cursor left/right and insert edits input correctly", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Send chars and control sequences:
    // Step 1: insert 'a', 'b', 'c' => buffer: "abc", cursor at end
    test_send_characters(s_socket_fd_a[1], "abc");

    // Step 2: CTRL-B (move cursor left once), cursor between 'b' and 'c'
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // Step 3: insert 'X' => buffer: "abXc"
    test_send_characters(s_socket_fd_a[1], "X");

    // Step 4: CTRL-F (move cursor right), cursor after 'X'
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_F));

    // Step 5: insert 'Y' => buffer: "abXcY"
    test_send_characters(s_socket_fd_a[1], "Y");

    // Step 6: send newline to finish input
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // The line returned should be "abXcY"
    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abXcY", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-A moves cursor home, CTRL-E moves cursor end, inserts work correctly", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // ----- Test CTRL-A: move home -----
    // Insert 'bcd'
    test_send_characters(s_socket_fd_a[1], "bcd");

    // CTRL-A: move cursor to start
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_A));

    // Insert 'a' at beginning → "abcd"
    test_send_characters(s_socket_fd_a[1], "a");

    // CTRL-E to move to end
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_E));

    // Insert 'e' at end → "abcde"
    test_send_characters(s_socket_fd_a[1], "e");

    // send new line character
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abcde", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("history navigation with CTRL-P / CTRL-N works correctly", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    wait_ms(100);

    // Add history entries in order: "first", "second", "third"
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "first"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "second"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "third"));

    wait_ms(100);

    // CTRL-P: get previous history command, should be "third"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_P));

    // CTRL-P: get previous history command, should be "second"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_P));

    // extend the "second" line to see if when we go back to it,
    // it will print the updated line or the former line
    test_send_characters(s_socket_fd_a[1], "second");

    // CTRL-P: get previous history command, should be "first"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_P));

    // CTRL-N: get previous history command, should be "second"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_N));

    // Send newline to accept current line
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Expect "third" as final returned line after navigation
    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("secondsecond", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("backspace erases the character before the cursor", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abc"
    test_send_characters(s_socket_fd_a[1], "abc");

    // BACKSPACE (127), buffer becomes "ab"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(BACKSPACE));

    // CTRL-H (8), buffer becomes "a"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_H));

    // Insert "aa", buffer becomes "aaa"
    test_send_characters(s_socket_fd_a[1], "aa");

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("aaa", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-D removes character at the right of the cursor", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters(s_socket_fd_a[1], "abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-D, buffer becomes "abde"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_D));

    // CTRL-D, buffer becomes "abe"
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_D));

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abe", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-T swaps character with previous", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters(s_socket_fd_a[1], "abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-T (swap characters)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_T));

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abced", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-U deletes the whole line", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters(s_socket_fd_a[1], "abcde");

    // CTRL-U (delete all line)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_U));

    // Insert "fghij"
    test_send_characters(s_socket_fd_a[1], "fghij");

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("fghij", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-K deletes from character to end of line", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters(s_socket_fd_a[1], "abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_B));

    // CTRL-K (delete from current character on)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_K));

    // Insert "abab"
    test_send_characters(s_socket_fd_a[1], "abab");

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("ababab", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-L clears the screen", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // CTRL-L (clear screen)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_L));

    // we can't check that the screen is actually cleared but
    // we can check that the proper command was sent from
    // linenoise to the terminal
    // Verify prompt string is found in output
    wait_ms(50);
    char full_cmd_line[32] = {0};
    const char *expect_string = "screen cleared";
    const ssize_t nread = read(s_socket_fd_a[1], full_cmd_line, 32);
    TEST_ASSERT_NOT_EQUAL(-1, nread);
    TEST_ASSERT_NOT_NULL(strstr(full_cmd_line, expect_string));

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("CTRL-W removes the previous word", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "word_a "
    test_send_characters(s_socket_fd_a[1], "word_a");
    test_send_characters(s_socket_fd_a[1], " ");

    // Insert "word_b"
    test_send_characters(s_socket_fd_a[1], "word_b");

    // CTRL-W (removes previous work)
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(CTRL_W));

    // Insert "word_c"
    test_send_characters(s_socket_fd_a[1], "word_c");

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("word_a word_c", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("check completion, hint and free hint callback", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    config.completion_cb = custom_completion_cb;
    config.hints_cb = custom_hint_cb;
    config.free_hints_cb = custom_free_hint_cb;

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "word_a" this should trigger the hint cb and free hints cb
    test_send_characters(s_socket_fd_a[1], "word_a");

    // TAB: this should trigger the completions cb
    test_send_characters(s_socket_fd_a[1], COMPOUND_LITERAL(TAB));

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_EQUAL(true, s_hint_called);
    TEST_ASSERT_EQUAL(true, s_completions_called);
    TEST_ASSERT_EQUAL(true, s_free_hint_called);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("check esp_linenoise_get_line return values", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    s_linenoise_hdl = esp_linenoise_create_instance(&config);
    TEST_ASSERT_NOT_NULL(s_linenoise_hdl);

    const size_t buffer_size = 10;
    char buffer[buffer_size];

    // pass NULL buffer, expect invalid arg error
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_linenoise_get_line(s_linenoise_hdl, NULL, buffer_size));
    // pass 0 buffer size, expect invalid arg error
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_linenoise_get_line(s_linenoise_hdl, buffer, 0));
    // pass buffer size bigger than max cmd line length, expect invalid arg error
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_linenoise_get_line(s_linenoise_hdl, buffer, config.max_cmd_line_length + 1));

    // update the value of allow_empty_line to false and send an empty
    // line, expect esp_linenoise_get_line to return with ESP_FAIL
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_empty_line(s_linenoise_hdl, false));

    get_line_task_args_t args = {
        .handle = s_linenoise_hdl,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock,
        .ret_val = ESP_OK,
        .buf = buffer,
        .buf_size = buffer_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Newline (accept)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // check that esp_linenoise_get_line returned ESP_FAIL
    TEST_ASSERT_EQUAL(ESP_FAIL, args.ret_val);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

TEST_CASE("check cmd line is bigger than the buffer", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    s_linenoise_hdl = esp_linenoise_create_instance(&config);
    TEST_ASSERT_NOT_NULL(s_linenoise_hdl);

    const size_t buffer_size = 10;
    char buffer[buffer_size];

    // update the value of allow_empty_line to false and send an empty
    // line, expect esp_linenoise_get_line to return with ESP_FAIL
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_empty_line(s_linenoise_hdl, false));

    get_line_task_args_t args = {
        .handle = s_linenoise_hdl,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock,
        .ret_val = ESP_OK,
        .buf = buffer,
        .buf_size = buffer_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // send more characters than the size of the buffer when linenoise
    // has dumb mode turned off
    test_send_characters(s_socket_fd_a[1], "aaaaaaaaaaa\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // check that esp_linenoise_get_line returned ESP_OK
    TEST_ASSERT_EQUAL(ESP_OK, args.ret_val);
    // linenoise should return when size of buffer - 1 is filled
    TEST_ASSERT_EQUAL(buffer_size - 1, strlen(buffer));

    // reset the buffer and release the mutex
    pthread_mutex_unlock(&lock);
    memset(buffer, 0, buffer_size);

    // switch the dumb mode on
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_dumb_mode(s_linenoise_hdl, true));

    // repeat the test
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args, 5, NULL);

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // send more characters than the size of the buffer when linenoise
    // has dumb mode turned on
    test_send_characters(s_socket_fd_a[1], "aaaaaaaaaaa\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // check that esp_linenoise_get_line returned ESP_OK and the
    // number of char is equal to buffer_size - 1
    TEST_ASSERT_EQUAL(ESP_OK, args.ret_val);
    TEST_ASSERT_EQUAL(buffer_size - 1, strlen(buffer));

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* Mappings from shortkey actions to escape sequences:
   - Up arrow     : "\x1b[A"
   - Down arrow   : "\x1b[B"
   - Right arrow  : "\x1b[C"
   - Left arrow   : "\x1b[D"
   - Home         : "\x1b[H" or "\x1bOH"
   - End          : "\x1b[F" or "\x1bOF"
   - Delete key   : "\x1b[3~"
*/

TEST_CASE("cursor left/right edits work via escape sequences", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);
    pthread_mutex_lock(&lock);

    test_send_characters(s_socket_fd_a[1], "abc");       // step 1: insert abc
    test_send_characters(s_socket_fd_a[1], "\x1b[D");    // step 2: left arrow (cursor between b and c)
    test_send_characters(s_socket_fd_a[1], "X");         // step 3: insert X
    test_send_characters(s_socket_fd_a[1], "\x1b[C");    // step 4: right arrow
    test_send_characters(s_socket_fd_a[1], "Y");         // step 5: insert Y
    test_send_characters(s_socket_fd_a[1], "\n");        // finish input

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TEST_ASSERT_EQUAL_STRING("abXcY", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* Home and End via escape sequences */
TEST_CASE("Home and End keys work via escape sequences", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);
    pthread_mutex_lock(&lock);

    test_send_characters(s_socket_fd_a[1], "bcd");       // buffer: bcd
    test_send_characters(s_socket_fd_a[1], "\x1b[H");    // Home key
    test_send_characters(s_socket_fd_a[1], "a");         // -> abcd
    test_send_characters(s_socket_fd_a[1], "\x1b[F");    // End key
    test_send_characters(s_socket_fd_a[1], "e");         // -> abcde
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TEST_ASSERT_EQUAL_STRING("abcde", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* History navigation with Up/Down arrows */
TEST_CASE("history navigation works via arrow keys", "[esp_linenoise][history]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);
    pthread_mutex_lock(&lock);

    // add history
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "first"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "second"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(s_linenoise_hdl, "third"));

    // navigate
    test_send_characters(s_socket_fd_a[1], "\x1b[A");    // up -> third
    test_send_characters(s_socket_fd_a[1], "\x1b[A");    // up -> second
    test_send_characters(s_socket_fd_a[1], "second");    // append text
    test_send_characters(s_socket_fd_a[1], "\x1b[A");    // up -> first
    test_send_characters(s_socket_fd_a[1], "\x1b[B");    // down -> secondsecond
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TEST_ASSERT_EQUAL_STRING("secondsecond", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* Delete key via escape sequence */
TEST_CASE("Delete key works via escape sequence", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);
    pthread_mutex_lock(&lock);

    test_send_characters(s_socket_fd_a[1], "abcde");
    test_send_characters(s_socket_fd_a[1], "\x1b[D");    // left
    test_send_characters(s_socket_fd_a[1], "\x1b[D");    // left
    test_send_characters(s_socket_fd_a[1], "\x1b[D");    // left
    test_send_characters(s_socket_fd_a[1], "\x1b[3~");   // delete (removes c)
    test_send_characters(s_socket_fd_a[1], "\x1b[3~");   // delete (removes d)
    test_send_characters(s_socket_fd_a[1], "\n");

    // wait for the task to terminate to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TEST_ASSERT_EQUAL_STRING("abe", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* Alternate Home/End sequences using ESC O form */
TEST_CASE("Home and End via ESC O form", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock, &config);

    get_line_args_t args = { .lock = &lock, .parent_task = xTaskGetCurrentTaskHandle(), .config = &config };
    xTaskCreate(get_line_task, "freertos_task", 2048, &args, 5, NULL);
    pthread_mutex_lock(&lock);

    test_send_characters(s_socket_fd_a[1], "bcd");
    test_send_characters(s_socket_fd_a[1], "\x1bOH");    // ESC O H for home
    test_send_characters(s_socket_fd_a[1], "a");
    test_send_characters(s_socket_fd_a[1], "\x1bOF");    // ESC O F for end
    test_send_characters(s_socket_fd_a[1], "e");
    test_send_characters(s_socket_fd_a[1], "\n");

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TEST_ASSERT_EQUAL_STRING("abcde", s_line_returned);

    test_instance_teardown(s_socket_fd_a, s_linenoise_hdl, &lock);
}

/* test multi instances */
TEST_CASE("Create and use 2 esp_linenoise instances", "[esp_linenoise]")
{
    esp_linenoise_config_t config_a;
    pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock_a, &config_a);
    esp_linenoise_handle_t linenoise_handle_a = esp_linenoise_create_instance(&config_a);
    TEST_ASSERT_NOT_NULL(linenoise_handle_a);

    const size_t buffer_a_size = 32;
    char buffer_a[buffer_a_size];
    memset(buffer_a, 0, buffer_a_size);

    get_line_task_args_t args_a = {
        .handle = linenoise_handle_a,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock_a,
        .ret_val = ESP_OK,
        .buf = buffer_a,
        .buf_size = buffer_a_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args_a, 5, NULL);
    pthread_mutex_lock(&lock_a);

    esp_linenoise_config_t config_b;
    pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_b, &lock_b, &config_b);
    esp_linenoise_handle_t linenoise_handle_b = esp_linenoise_create_instance(&config_b);
    TEST_ASSERT_NOT_NULL(linenoise_handle_b);

    const size_t buffer_b_size = 32;
    char buffer_b[buffer_b_size];
    memset(buffer_b, 0, buffer_b_size);

    get_line_task_args_t args_b = {
        .handle = linenoise_handle_b,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock_b,
        .ret_val = ESP_OK,
        .buf = buffer_b,
        .buf_size = buffer_b_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args_b, 5, NULL);
    pthread_mutex_lock(&lock_b);

    /* send different string to the instances and make sure each instances
     * get the correct string from the correct stream */
    const char *test_msg_a = "test_msg_a";
    const char *test_msg_b = "test_msg_b";
    test_send_characters(s_socket_fd_a[1], test_msg_a);
    test_send_characters(s_socket_fd_a[1], "\n");
    test_send_characters(s_socket_fd_b[1], test_msg_b);
    test_send_characters(s_socket_fd_b[1], "\n");


    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    TEST_ASSERT_EQUAL_STRING(test_msg_a, args_a.buf);
    TEST_ASSERT_EQUAL_STRING(test_msg_b, args_b.buf);

    test_instance_teardown(s_socket_fd_a, linenoise_handle_a, &lock_a);
    test_instance_teardown(s_socket_fd_b, linenoise_handle_b, &lock_b);
}

TEST_CASE("tests that esp_linenoise_abort actually forces esp_linenoise_get_line to return", "[esp_linenoise]")
{
    esp_linenoise_config_t config_a, config_b;
    pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;

    test_instance_setup(s_socket_fd_a, &lock_a, &config_a);
    test_instance_setup(s_socket_fd_b, &lock_b, &config_b);

    /* make sure to use the default read function */
    config_a.read_bytes_cb = NULL;
    config_b.read_bytes_cb = NULL;

    esp_linenoise_handle_t linenoise_handle_a = esp_linenoise_create_instance(&config_a);
    TEST_ASSERT_NOT_NULL(linenoise_handle_a);

    const size_t buffer_a_size = 32;
    char buffer_a[buffer_a_size];
    memset(buffer_a, 0, buffer_a_size);

    get_line_task_args_t args_a = {
        .handle = linenoise_handle_a,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock_a,
        .ret_val = ESP_OK,
        .buf = buffer_a,
        .buf_size = buffer_a_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args_a, 5, NULL);
    pthread_mutex_lock(&lock_a);

    esp_linenoise_handle_t linenoise_handle_b = esp_linenoise_create_instance(&config_b);
    TEST_ASSERT_NOT_NULL(linenoise_handle_b);

    const size_t buffer_b_size = 32;
    char buffer_b[buffer_b_size];
    memset(buffer_b, 0, buffer_b_size);

    get_line_task_args_t args_b = {
        .handle = linenoise_handle_b,
        .parent_task = xTaskGetCurrentTaskHandle(),
        .lock = &lock_b,
        .ret_val = ESP_OK,
        .buf = buffer_b,
        .buf_size = buffer_b_size
    };
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args_b, 5, NULL);
    pthread_mutex_lock(&lock_b);



    /* send test message to instance */
    const char dummy_message[] = "dummy_message";
    test_send_characters(s_socket_fd_a[1], dummy_message);

    /* for the esp_linenoise to process the message */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* call the esp_linenoise_abort on linenoise_handle_a to return from esp_linenoise_get_line */
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_abort(linenoise_handle_a));

    /* wait for the task running the linenoise instance A to return */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* check that the message was processed by the instance A */
    TEST_ASSERT_EQUAL_STRING(dummy_message, args_a.buf);



    /* send dummy message to instance B, that should still be running */
    test_send_characters(s_socket_fd_b[1], dummy_message);

    /* for the esp_linenoise to process the message */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* call the esp_linenoise_abort on linenoise_handle_a to return from esp_linenoise_get_line */
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_abort(linenoise_handle_b));

    /* wait for the task running the linenoise instance A to return */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* check that the message was processed by the instance B */
    TEST_ASSERT_EQUAL_STRING(dummy_message, args_b.buf);



    /* start instance A and repeat test to make sure it is possible to restart an instance
     * even after aborting it */
    xTaskCreate(get_line_task_w_args, "freertos_task", 2048, &args_a, 5, NULL);
    pthread_mutex_lock(&lock_a);
    test_send_characters(s_socket_fd_a[1], dummy_message);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_abort(linenoise_handle_a));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    test_instance_teardown(s_socket_fd_a, linenoise_handle_a, &lock_a);
    test_instance_teardown(s_socket_fd_b, linenoise_handle_b, &lock_b);
}
