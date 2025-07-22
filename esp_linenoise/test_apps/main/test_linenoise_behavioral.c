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
#include "fcntl.h"
#include "unity.h"
#include "esp_linenoise.h"
#include "test_linenoise_utils.h"

static int s_socket_fd[2];
static esp_linenoise_handle_t s_linenoise_hdl;
static char s_line_returned[CMD_LINE_LENGTH] = {0};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool completions_called = false;
static void custom_completion_cb(void *user_ctx, const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    // we just want to see that the callback is indeed called
    // so flip the completions_called to true
    if (!completions_called) {
        completions_called = true;
    }
}

static bool hint_called = false;
static char *custom_hint_cb(void *user_ctx, const char *str, int *color, int *bold)
{
    // we just want to see that the callback is indeed called
    // so flip the hint_called to true
    if (!hint_called) {
        hint_called = true;
    }

    return "something";
}

static bool free_hint_called = false;
static void custom_free_hint_cb(void *user_ctx, void *ptr)
{
    // we just want to see that the callback is indeed called
    // so flip the free_hint_called to true
    if (!free_hint_called) {
        free_hint_called = true;
    }
}

static ssize_t custom_read(void *user_ctx, int fd, void *buf, size_t count)
{
    // otherwise just propagate the read
    (void)user_ctx;
    int nread = read(fd, buf, count);

    // printf("reading : ");
    // for (size_t i = 0; i < nread; i++) {
    //     printf("(%x, %c) ", *((char*)buf + i), *((char*)buf + i));
    // }
    // printf("\n");

    return nread;
}

static ssize_t custom_write(void *user_ctx, int fd, const void *buf, size_t count)
{
    // printf("writing : ");
    // for (size_t i = 0; i < count; i++) {
    //     printf("(%x, %c) ", *((char*)buf + i), *((char*)buf + i));
    // }
    // printf("\n");

    // find the request in the list of commands and send the response
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
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

static void test_setup(esp_linenoise_config_t *config)
{
    // 2 fd are generated, simulating the full-duplex
    // communication between linenoise and the terminal
    socketpair(AF_UNIX, SOCK_STREAM, 0, s_socket_fd);

    // assure that the read will be blocking
    int flags = fcntl(s_socket_fd[0], F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(s_socket_fd[0], F_SETFL, flags);

    esp_linenoise_get_instance_config_default(config);

    // all data written by the test on s_socket_fd[1] will
    // bbe available for reading on s_socket_fd[0]. All data
    // written to s_socket_fd[0] will be available for reading
    // ons_socket_fd[1].
    config->in_fd = s_socket_fd[0];
    config->out_fd = s_socket_fd[0];

    // redirect read and write calls from linenoise to be able
    // to e.g., process sequences sent from linenoise and thus
    // simulate that the terminal supports escape sequences
    config->read_bytes_cb = custom_read;
    config->write_bytes_cb = custom_write;

    // take the semaphore
    pthread_mutex_lock(&lock);
}

static void test_teardown(void)
{
    memset(s_line_returned, 0, CMD_LINE_LENGTH);

    esp_linenoise_delete_instance(s_linenoise_hdl);
    close(s_socket_fd[0]);
    close(s_socket_fd[1]);

    // unlock the mutex for the next test
    pthread_mutex_unlock(&lock);
}

static void test_send_characters(const char *msg)
{
    // wait to simulate that the user is doing the input
    // and prevent linenoise to detect the incoming character(s)
    // as pasted
    wait_ms(50);

    const size_t msg_len = strlen(msg);
    const int nwrite = write(s_socket_fd[1], msg, msg_len);
    TEST_ASSERT_EQUAL(msg_len, nwrite);
}

static void *get_line_task(void *arg)
{
    esp_linenoise_config_t *config = (esp_linenoise_config_t *)arg;
    s_linenoise_hdl = esp_linenoise_create_instance(config);
    TEST_ASSERT_NOT_NULL(s_linenoise_hdl);

    // wait for the instance to properly initialize before unlocking
    // the mutex so the test can run
    usleep(100000);

    // release the mutex so the test can start sending data
    pthread_mutex_unlock(&lock);

    esp_err_t ret_val = esp_linenoise_get_line(s_linenoise_hdl, s_line_returned, CMD_LINE_LENGTH);
    TEST_ASSERT_EQUAL(ESP_OK, ret_val);
    return NULL;
}

typedef struct get_line_task_args {
    esp_err_t ret_val;
    char *buf;
    size_t buf_size;
} get_line_task_args_t;

static void *get_line_task_w_args(void *args)
{
    get_line_task_args_t *task_args = (get_line_task_args_t *)args;

    // wait for the instance to properly initialize before unlocking
    // the mutex so the test can run
    usleep(100000);

    // release the mutex so the test can start sending data
    pthread_mutex_unlock(&lock);

    task_args->ret_val = esp_linenoise_get_line(s_linenoise_hdl, task_args->buf, task_args->buf_size);
    return NULL;
}

TEST_CASE("esp_linenoise_get_line() returns line read from in_fd", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    const char *input_line = "unit test input";
    test_send_characters(input_line);

    // Write newline to trigger prompt output + return from loop
    test_send_characters("\n");

    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_NOT_NULL(strstr(input_line, s_line_returned));

    test_teardown();
}

TEST_CASE("custom prompt string appears on output", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    const char *custom_prompt = ">>> ";
    config.prompt = (char *)custom_prompt;  // cast away const as config expects char*

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Write newline to trigger prompt output + return from loop
    test_send_characters("\n");

    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    // Verify prompt string is found in output
    char full_cmd_line[32] = {0};
    const ssize_t nread = read(s_socket_fd[1], full_cmd_line, 32);
    TEST_ASSERT_NOT_EQUAL(-1, nread);

    TEST_ASSERT_NOT_NULL(strstr(full_cmd_line, custom_prompt));

    test_teardown();
}

TEST_CASE("cursor left/right and insert edits input correctly", "[linenoise][]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Send chars and control sequences:
    // Step 1: insert 'a', 'b', 'c' => buffer: "abc", cursor at end
    test_send_characters("abc");

    // Step 2: CTRL-B (move cursor left once), cursor between 'b' and 'c'
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // Step 3: insert 'X' => buffer: "abXc"
    test_send_characters("X");

    // Step 4: CTRL-F (move cursor right), cursor after 'X'
    test_send_characters(COMPOUND_LITERAL(CTRL_F));

    // Step 5: insert 'Y' => buffer: "abXcY"
    test_send_characters("Y");

    // Step 6: send newline to finish input
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    // The line returned should be "abXcY"
    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abXcY", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-A moves cursor home, CTRL-E moves cursor end, inserts work correctly", "[edit][cursor][home][end]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // ----- Test CTRL-A: move home -----
    // Insert 'bcd'
    test_send_characters("bcd");

    // CTRL-A: move cursor to start
    test_send_characters(COMPOUND_LITERAL(CTRL_A));

    // Insert 'a' at beginning → "abcd"
    test_send_characters("a");

    // CTRL-E to move to end
    test_send_characters(COMPOUND_LITERAL(CTRL_E));

    // Insert 'e' at end → "abcde"
    test_send_characters("e");

    // send new line character
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abcde", s_line_returned);

    test_teardown();
}

TEST_CASE("history navigation with CTRL-P / CTRL-N works correctly", "[edit][history]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

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
    test_send_characters(COMPOUND_LITERAL(CTRL_P));

    // CTRL-P: get previous history command, should be "second"
    test_send_characters(COMPOUND_LITERAL(CTRL_P));

    // extend the "second" line to see if when we go back to it,
    // it will print the updated line or the former line
    test_send_characters("second");

    // CTRL-P: get previous history command, should be "first"
    test_send_characters(COMPOUND_LITERAL(CTRL_P));

    // CTRL-N: get previous history command, should be "second"
    test_send_characters(COMPOUND_LITERAL(CTRL_N));

    // Send newline to accept current line
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    // Expect "third" as final returned line after navigation
    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("secondsecond", s_line_returned);

    test_teardown();
}

TEST_CASE("backspace erases the character before the cursor", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abc"
    test_send_characters("abc");

    // BACKSPACE (127), buffer becomes "ab"
    test_send_characters(COMPOUND_LITERAL(BACKSPACE));

    // CTRL-H (8), buffer becomes "a"
    test_send_characters(COMPOUND_LITERAL(CTRL_H));

    // Insert "aa", buffer becomes "aaa"
    test_send_characters("aa");

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("aaa", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-D removes character at the right of the cursor", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters("abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-D, buffer becomes "abde"
    test_send_characters(COMPOUND_LITERAL(CTRL_D));

    // CTRL-D, buffer becomes "abe"
    test_send_characters(COMPOUND_LITERAL(CTRL_D));

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abe", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-T swaps character with previous", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters("abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-T (swap characters)
    test_send_characters(COMPOUND_LITERAL(CTRL_T));

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("abced", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-U deletes the whole line", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters("abcde");

    // CTRL-U (delete all line)
    test_send_characters(COMPOUND_LITERAL(CTRL_U));

    // Insert "fghij"
    test_send_characters("fghij");

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("fghij", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-K deletes from character to end of line", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "abcde"
    test_send_characters("abcde");

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-B (move cursor left once)
    test_send_characters(COMPOUND_LITERAL(CTRL_B));

    // CTRL-K (delete from current character on)
    test_send_characters(COMPOUND_LITERAL(CTRL_K));

    // Insert "abab"
    test_send_characters("abab");

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("ababab", s_line_returned);

    test_teardown();
}

TEST_CASE("CTRL-L clears the screen", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // CTRL-L (clear screen)
    test_send_characters(COMPOUND_LITERAL(CTRL_L));

    // we can't check that the screen is actually cleared but
    // we can check that the proper command was sent from
    // linenoise to the terminal
    // Verify prompt string is found in output
    wait_ms(50);
    char full_cmd_line[32] = {0};
    const char *expect_string = "screen cleared";
    const ssize_t nread = read(s_socket_fd[1], full_cmd_line, 32);
    TEST_ASSERT_NOT_EQUAL(-1, nread);
    TEST_ASSERT_NOT_NULL(strstr(full_cmd_line, expect_string));

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    test_teardown();
}

TEST_CASE("CTRL-W removes the previous word", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "word_a "
    test_send_characters("word_a");
    test_send_characters(" ");

    // Insert "word_b"
    test_send_characters("word_b");

    // CTRL-W (removes previous work)
    test_send_characters(COMPOUND_LITERAL(CTRL_W));

    // Insert "word_c"
    test_send_characters("word_c");

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_NOT_NULL(s_line_returned);
    TEST_ASSERT_EQUAL_STRING("word_a word_c", s_line_returned);

    test_teardown();
}

TEST_CASE("check completion, hint and free hint callback", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    config.completion_cb = custom_completion_cb;
    config.hints_cb = custom_hint_cb;
    config.free_hints_cb = custom_free_hint_cb;

    pthread_t thread_id;
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task, &config));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Insert "word_a" this should trigger the hint cb and free hints cb
    test_send_characters("word_a");

    // TAB: this should trigger the completions cb
    test_send_characters(COMPOUND_LITERAL(TAB));

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    TEST_ASSERT_EQUAL(true, hint_called);
    TEST_ASSERT_EQUAL(true, completions_called);
    TEST_ASSERT_EQUAL(true, free_hint_called);

    test_teardown();
}

TEST_CASE("check esp_linenoise_get_line return values", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

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

    pthread_t thread_id;
    get_line_task_args_t args = {
        .ret_val = ESP_OK,
        .buf = buffer,
        .buf_size = buffer_size
    };
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task_w_args, &args));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // Newline (accept)
    test_send_characters("\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    // check that esp_linenoise_get_line returned ESP_FAIL
    TEST_ASSERT_EQUAL(ESP_FAIL, args.ret_val);

    test_teardown();
}

TEST_CASE("check cmd line is bigger than the buffer", "[linenoise]")
{
    esp_linenoise_config_t config;
    test_setup(&config);

    s_linenoise_hdl = esp_linenoise_create_instance(&config);
    TEST_ASSERT_NOT_NULL(s_linenoise_hdl);

    const size_t buffer_size = 10;
    char buffer[buffer_size];

    // update the value of allow_empty_line to false and send an empty
    // line, expect esp_linenoise_get_line to return with ESP_FAIL
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_empty_line(s_linenoise_hdl, false));

    pthread_t thread_id;
    get_line_task_args_t args = {
        .ret_val = ESP_OK,
        .buf = buffer,
        .buf_size = buffer_size
    };
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task_w_args, &args));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // send more characters than the size of the buffer when linenoise
    // has dumb mode turned off
    test_send_characters("aaaaaaaaaaa\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

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
    TEST_ASSERT_EQUAL(0, pthread_create(&thread_id, NULL, get_line_task_w_args, &args));

    // wait until the linenoise instance init is done, and the get line as started
    // before sending test content
    pthread_mutex_lock(&lock);

    // send more characters than the size of the buffer when linenoise
    // has dumb mode turned on
    test_send_characters("aaaaaaaaaaa\n");

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(0, pthread_join(thread_id, NULL));

    // check that esp_linenoise_get_line returned ESP_OK and the
    // number of char is equal to buffer_size - 1
    TEST_ASSERT_EQUAL(ESP_OK, args.ret_val);
    TEST_ASSERT_EQUAL(buffer_size - 1, strlen(buffer));

    test_teardown();
}
