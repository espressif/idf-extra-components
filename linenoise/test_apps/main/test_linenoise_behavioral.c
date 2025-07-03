#include <string.h>
#include <stdio.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fcntl.h"
#include "linenoise/linenoise.h"

static int in_pipe[2];
static int out_pipe[2];
static linenoise_handle_t h;
static char *line_returned = NULL;
static SemaphoreHandle_t done_sem;

static void linenoise_loop_task(void *arg)
{
    line_returned = linenoise_loop(h);
    xSemaphoreGive(done_sem);
    vTaskDelete(NULL);
}

// Check in_fd and out_fd

TEST_CASE("linenoise: linenoise_loop() returns line read from in_fd", "[linenoise][fd][loop]")
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    // Make output pipe non-blocking so read won't hang
    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(50)); // allow loop init

    const char *input_line = "unit test input\n";
    ssize_t written = write(in_pipe[1], input_line, strlen(input_line));
    TEST_ASSERT_EQUAL(strlen(input_line), written);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));

    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING_LEN(input_line, line_returned, strlen(input_line) - 1);

    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);

    vSemaphoreDelete(done_sem);
    done_sem = NULL;
}

// Check prompt

static const char *custom_prompt = ">>> ";

TEST_CASE("linenoise: custom prompt string appears on output", "[linenoise][prompt][fd][loop]")
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];
    param.prompt = (char *)custom_prompt;  // cast away const as param expects char*

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(50)); // allow loop init

    // Write newline to trigger prompt output + return from loop
    const char *input_line = "\n";
    ssize_t written = write(in_pipe[1], input_line, strlen(input_line));
    TEST_ASSERT_EQUAL(strlen(input_line), written);

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));

    // Read output from pipe (prompt should be output first)
    char output_buf[128] = {0};
    ssize_t n = read(out_pipe[0], output_buf, sizeof(output_buf) - 1);
    TEST_ASSERT_GREATER_THAN(0, n);
    output_buf[n] = '\0';

    // Verify prompt string is found in output
    TEST_ASSERT_NOT_NULL(strstr(output_buf, custom_prompt));

    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);

    vSemaphoreDelete(done_sem);
    done_sem = NULL;
}

// test move cursor left / right

#define CTRL_B 0x02
#define CTRL_F 0x06

TEST_CASE("linenoise: cursor left/right and insert edits input correctly", "[edit][cursor][loop]")
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    // Non-blocking read on output pipe
    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];
    param.prompt = NULL;

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(50)); // let loop start

    // Send chars and control sequences:
    // Step 1: insert 'a', 'b', 'c' => buffer: "abc", cursor at end
    write(in_pipe[1], "abc", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 2: CTRL-B (move cursor left once), cursor between 'b' and 'c'
    char ctrl_b = CTRL_B;
    write(in_pipe[1], &ctrl_b, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 3: insert 'X' => buffer: "abXc"
    write(in_pipe[1], "X", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 4: CTRL-F (move cursor right), cursor after 'X'
    char ctrl_f = CTRL_F;
    write(in_pipe[1], &ctrl_f, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 5: insert 'Y' => buffer: "abXcY"
    write(in_pipe[1], "Y", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 6: send newline to finish input
    write(in_pipe[1], "\n", 1);

    // Wait for loop to finish
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));

    // The line returned should be "abXcY"
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("abXcY", line_returned);

    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);

    vSemaphoreDelete(done_sem);
    done_sem = NULL;
}

// test move end / move home

TEST_CASE("linenoise: CTRL-A moves cursor home, CTRL-E moves cursor end, inserts work correctly", "[edit][cursor][home][end]")
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];
    param.prompt = NULL;

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));

    // ----- Test CTRL-A: move home -----
    // Insert 'bcd'
    write(in_pipe[1], "bcd", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-A: move cursor to start
    char ctrl_a = 0x01;
    write(in_pipe[1], &ctrl_a, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'a' at beginning → "abcd"
    write(in_pipe[1], "a", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Send newline to finish input
    write(in_pipe[1], "\n", 1);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));

    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("abcd", line_returned);
    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);

    // ----- Setup again for CTRL-E test -----
    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Insert 'abc'
    write(in_pipe[1], "abc", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-A to move to start
    write(in_pipe[1], &ctrl_a, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'X' at start → "Xabc"
    write(in_pipe[1], "X", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-E to move to end
    char ctrl_e = 0x05;
    write(in_pipe[1], &ctrl_e, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'Y' at end → "XabcY"
    write(in_pipe[1], "Y", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Send newline to finish input
    write(in_pipe[1], "\n", 1);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));

    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("XabcY", line_returned);
    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);

    vSemaphoreDelete(done_sem);
    done_sem = NULL;
}

// move up and down the history

TEST_CASE("linenoise: history navigation with arrow up/down works correctly", "[edit][history]")
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];
    param.prompt = NULL;

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    // Add history entries in order: "first", "second", "third"
    TEST_ASSERT_EQUAL(0, linenoise_history_add(h, "first"));
    TEST_ASSERT_EQUAL(0, linenoise_history_add(h, "second"));
    TEST_ASSERT_EQUAL(0, linenoise_history_add(h, "third"));

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Simulate arrow up: ESC [ A (history previous)
    char arrow_up_seq[] = {0x1B, 0x5B, 0x41};
    write(in_pipe[1], arrow_up_seq, sizeof(arrow_up_seq));
    vTaskDelay(pdMS_TO_TICKS(20));  // now line should be "third" (most recent)

    // Press arrow up again, should get "second"
    write(in_pipe[1], arrow_up_seq, sizeof(arrow_up_seq));
    vTaskDelay(pdMS_TO_TICKS(20));

    // Simulate arrow down: ESC [ B (history next)
    char arrow_down_seq[] = {0x1B, 0x5B, 0x42};
    write(in_pipe[1], arrow_down_seq, sizeof(arrow_down_seq));  // back to "third"
    vTaskDelay(pdMS_TO_TICKS(20));

    // Send newline to accept current line
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);

    // Expect "third" as final returned line after navigation
    TEST_ASSERT_EQUAL_STRING("third", line_returned);

    free(line_returned);
    line_returned = NULL;

    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);

    vSemaphoreDelete(done_sem);
    done_sem = NULL;
}





static void setup_linenoise_instance(void)
{
    TEST_ASSERT_EQUAL(0, pipe(in_pipe));
    TEST_ASSERT_EQUAL(0, pipe(out_pipe));

    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    param.in_fd = in_pipe[0];
    param.out_fd = out_pipe[1];
    param.prompt = NULL;

    h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);

    done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    xTaskCreate(linenoise_loop_task, "linenoise_loop_task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void teardown_linenoise_instance(void)
{
    linenoise_delete_instance(h);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    vSemaphoreDelete(done_sem);
    done_sem = NULL;
    line_returned = NULL;
}

TEST_CASE("linenoise: cursor movement left/right with insert and accept line", "[edit][cursor]")
{
    setup_linenoise_instance();

    // Insert 'a', 'c' (buffer: "ac")
    write(in_pipe[1], "ac", 2);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-B (0x02) move cursor left (between 'a' and 'c')
    char ctrl_b = 0x02;
    write(in_pipe[1], &ctrl_b, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'b' (should go between 'a' and 'c': "abc")
    write(in_pipe[1], "b", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-F (0x06) move cursor right (cursor after 'c')
    char ctrl_f = 0x06;
    write(in_pipe[1], &ctrl_f, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'd' (end of line: "abcd")
    write(in_pipe[1], "d", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Newline (accept line)
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("abcd", line_returned);

    free(line_returned);
    teardown_linenoise_instance();
}


TEST_CASE("linenoise: delete key removes char under cursor", "[edit][delete]")
{
    setup_linenoise_instance();

    // Insert "abc"
    write(in_pipe[1], "abc", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-B (move cursor left) from end, cursor now at 'c'
    char ctrl_b = 0x02;
    write(in_pipe[1], &ctrl_b, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // DEL (0x7F) deletes char under cursor ('c'), buffer becomes "ab"
    char del = 0x7F;
    write(in_pipe[1], &del, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Newline (accept)
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("ab", line_returned);

    free(line_returned);
    teardown_linenoise_instance();
}

TEST_CASE("linenoise: backspace removes char before cursor", "[edit][backspace]")
{
    setup_linenoise_instance();

    // Insert "abc"
    write(in_pipe[1], "abc", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // CTRL-B (move cursor left) cursor at 'c'
    char ctrl_b = 0x02;
    write(in_pipe[1], &ctrl_b, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Backspace (0x08) removes char before cursor ('b'), buffer becomes "ac"
    char backspace = 0x08;
    write(in_pipe[1], &backspace, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Newline accept
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("ac", line_returned);

    free(line_returned);
    teardown_linenoise_instance();
}

TEST_CASE("linenoise: Ctrl-L clears screen and continues editing", "[edit][clear_screen]")
{
    setup_linenoise_instance();

    // Insert "abc"
    write(in_pipe[1], "abc", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Ctrl-L (clear screen)
    char ctrl_l = 0x0C;
    write(in_pipe[1], &ctrl_l, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert "d"
    write(in_pipe[1], "d", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Newline accept
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("abcd", line_returned);

    free(line_returned);
    teardown_linenoise_instance();
}

TEST_CASE("linenoise: Ctrl-A and Ctrl-E move cursor home and end with insert", "[edit][cursor][home][end]")
{
    setup_linenoise_instance();

    // Insert 'bcd'
    write(in_pipe[1], "bcd", 3);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Ctrl-A (move to home)
    char ctrl_a = 0x01;
    write(in_pipe[1], &ctrl_a, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'a' at start → "abcd"
    write(in_pipe[1], "a", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Ctrl-E (move to end)
    char ctrl_e = 0x05;
    write(in_pipe[1], &ctrl_e, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Insert 'e' at end → "abcde"
    write(in_pipe[1], "e", 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Newline accept
    write(in_pipe[1], "\n", 1);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_NOT_NULL(line_returned);
    TEST_ASSERT_EQUAL_STRING("abcde", line_returned);

    free(line_returned);
    teardown_linenoise_instance();
}
