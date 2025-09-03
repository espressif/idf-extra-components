/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "esp_repl.h"
#include "esp_linenoise.h"
#include "esp_commands.h"

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
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* fake the section expected by esp_commands, make sure end and starts
 * at the same address so the esp_commands component sees no commands */
esp_command_t _esp_commands_start;
extern esp_command_t _esp_commands_end __attribute__((alias("_esp_commands_start")));

static int s_socket_fd[2];
static size_t s_pre_executor_nb_of_calls = 0;
static size_t s_post_executor_nb_of_calls = 0;
static size_t s_on_stop_nb_of_calls = 0;
static size_t s_on_exit_nb_of_calls = 0;

static void test_socket_setup(int socket_fd[2])
{
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fd));

    /* ensure reads are blocking */
    int flags = fcntl(socket_fd[0], F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(socket_fd[0], F_SETFL, flags);

    flags = fcntl(socket_fd[1], F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(socket_fd[1], F_SETFL, flags);
}

static void test_socket_teardown(int socket_fd[2])
{
    close(socket_fd[0]);
    close(socket_fd[1]);
}

static void test_send_characters(int socket_fd, const char *msg)
{
    wait_ms(100);

    const size_t msg_len = strlen(msg);
    const int nwrite = write(socket_fd, msg, msg_len);
    TEST_ASSERT_EQUAL(msg_len, nwrite);
}

esp_err_t test_pre_executor(void *ctx, char *buf, const esp_err_t reader_ret_val)
{
    s_pre_executor_nb_of_calls++;
    return ESP_OK;
}

esp_err_t test_post_executor(void *ctx, const char *buf, const esp_err_t executor_ret_val, const int cmd_ret_val)
{
    s_post_executor_nb_of_calls++;
    return ESP_OK;
}

void test_on_stop(void *ctx, esp_repl_handle_t handle)
{
    s_on_stop_nb_of_calls++;
    return;
}

void test_on_exit(void *ctx, esp_repl_handle_t handle)
{
    s_on_exit_nb_of_calls++;
    return;
}

/* Pass two semaphores:
 *  - start_sem: child gives it when it reached esp_repl (so main knows child started)
 *  - done_sem:   child gives it just before deleting itself (so main can "join")
 */
typedef struct task_args {
    SemaphoreHandle_t start_sem;
    SemaphoreHandle_t done_sem;
    esp_repl_handle_t hdl;
} task_args_t;

static void repl_task(void *args)
{
    task_args_t *task_args = (task_args_t *)args;

    /* signal to main that task started and esp_repl will run */
    xSemaphoreGive(task_args->start_sem);

    /* run the REPL loop (will return when stopped) */
    esp_repl(task_args->hdl);

    /* signal completion (emulates pthread_join notification) */
    xSemaphoreGive(task_args->done_sem);

    /* self-delete */
    vTaskDelete(NULL);
}

TEST_CASE("esp_repl() loop calls all callbacks and exit on call to esp_repl_stop", "[esp_repl]")
{
    /* create semaphores */
    SemaphoreHandle_t start_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(start_sem);
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    /* ensure both semaphores are in the "taken/empty" state:
       taking with 0 timeout guarantees they are empty afterwards
       regardless of the create semantics on this FreeRTOS build. */
    xSemaphoreTake(start_sem, 0);
    xSemaphoreTake(done_sem, 0);

    esp_linenoise_config_t linenoise_config;
    esp_linenoise_get_instance_config_default(&linenoise_config);
    test_socket_setup(s_socket_fd);
    linenoise_config.in_fd = s_socket_fd[0];
    linenoise_config.out_fd = s_socket_fd[0];
    esp_linenoise_handle_t esp_linenoise_hdl;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, &esp_linenoise_hdl));
    TEST_ASSERT_NOT_NULL(esp_linenoise_hdl);

    esp_repl_config_t repl_config = {
        .linenoise_handle = esp_linenoise_hdl,
        .command_set_handle = NULL,
        .max_cmd_line_size = 256,
        .history_save_path = NULL,
        .pre_executor = { .func = test_pre_executor, .ctx = NULL },
        .post_executor = { .func = test_post_executor, .ctx = NULL },
        .on_stop = { .func = test_on_stop, .ctx = NULL },
        .on_exit = { .func = test_on_exit, .ctx = NULL }
    };

    esp_repl_handle_t repl_handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_create(&repl_config, &repl_handle));
    TEST_ASSERT_NOT_NULL(repl_handle);

    task_args_t args = {.start_sem = start_sem, .done_sem = done_sem, .hdl = repl_handle};

    /* create the repl task */
    BaseType_t rc = xTaskCreate(repl_task, "repl_task", 4096, &args, 5, NULL);
    TEST_ASSERT_EQUAL(pdPASS, rc);

    /* should fail before repl is started */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    /* start repl */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_start(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_start(repl_handle));

    wait_ms(100);

    /* wait for the repl task to signal it started */
    TEST_ASSERT_TRUE(xSemaphoreTake(start_sem, pdMS_TO_TICKS(2000)));

    /* send a dummy string new line terminated to trigger linenoise to return */
    const char *input_line = "dummy_message\n";
    test_send_characters(s_socket_fd[1], input_line);

    /* wait for a bit so esp_repl() has time to loop back into esp_linenoise_get_line */
    wait_ms(100);

    /* check that pre-executor, post-executor callbacks are called */
    TEST_ASSERT_EQUAL(1, s_pre_executor_nb_of_calls);
    TEST_ASSERT_EQUAL(1, s_post_executor_nb_of_calls);

    /* stop repl and wait for task to finish (emulate pthread_join) */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    /* wait for the repl task to signal completion */
    TEST_ASSERT_TRUE(xSemaphoreTake(done_sem, pdMS_TO_TICKS(2000)));

    /* check that all callbacks were called the right number of times */
    TEST_ASSERT_EQUAL(1, s_on_stop_nb_of_calls);
    TEST_ASSERT_EQUAL(1, s_on_exit_nb_of_calls);
    TEST_ASSERT_EQUAL(2, s_pre_executor_nb_of_calls);
    TEST_ASSERT_EQUAL(2, s_post_executor_nb_of_calls);

    /* make sure calling stop fails because the repl is no longer running */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    /* reset the static variables */
    s_on_stop_nb_of_calls = 0;
    s_on_exit_nb_of_calls = 0;
    s_pre_executor_nb_of_calls = 0;
    s_post_executor_nb_of_calls = 0;

    /* destroy the repl instance */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_destroy(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_destroy(repl_handle));

    /* cleanup semaphores */
    vSemaphoreDelete(start_sem);
    vSemaphoreDelete(done_sem);

    test_socket_teardown(s_socket_fd);
}

static int quit_command(void *context, const int fd_out, int argc, char **argv)
{
    esp_repl_handle_t repl_handle = (esp_repl_handle_t)context;
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    return 0;
}

TEST_CASE("esp_repl() exits when esp_repl_stop() called from the task running esp_repl()", "[esp_repl]")
{
    /* create semaphores */
    SemaphoreHandle_t start_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(start_sem);
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    /* ensure both semaphores are in the "taken/empty" state:
       taking with 0 timeout guarantees they are empty afterwards
       regardless of the create semantics on this FreeRTOS build. */
    xSemaphoreTake(start_sem, 0);
    xSemaphoreTake(done_sem, 0);

    esp_linenoise_config_t linenoise_config;
    esp_linenoise_get_instance_config_default(&linenoise_config);
    test_socket_setup(s_socket_fd);
    linenoise_config.in_fd = s_socket_fd[0];
    linenoise_config.out_fd = s_socket_fd[0];
    esp_linenoise_handle_t esp_linenoise_hdl;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, &esp_linenoise_hdl));
    TEST_ASSERT_NOT_NULL(esp_linenoise_hdl);

    esp_repl_config_t repl_config = {
        .linenoise_handle = esp_linenoise_hdl,
        .command_set_handle = NULL,
        .max_cmd_line_size = 256,
        .history_save_path = NULL,
        .pre_executor = { .func = test_pre_executor, .ctx = NULL },
        .post_executor = { .func = test_post_executor, .ctx = NULL },
        .on_stop = { .func = test_on_stop, .ctx = NULL },
        .on_exit = { .func = test_on_exit, .ctx = NULL }
    };

    esp_repl_handle_t repl_handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_create(&repl_config, &repl_handle));
    TEST_ASSERT_NOT_NULL(repl_handle);

    task_args_t args = {.start_sem = start_sem, .done_sem = done_sem, .hdl = repl_handle};

    /* create the repl task */
    BaseType_t rc = xTaskCreate(repl_task, "repl_task", 4096, &args, 5, NULL);
    TEST_ASSERT_EQUAL(pdPASS, rc);

    /* register a quit command to esp_commands */
    esp_command_t quit_cmd = {
        .name = "quit",
        .group = "quit",
        .help = "-",
        .func = quit_command,
        .func_ctx = repl_handle,
        .hint_cb = NULL,
        .glossary_cb = NULL
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&quit_cmd));

    /* start repl */
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_start(repl_handle));

    wait_ms(100);

    /* wait for the repl task to signal it started */
    TEST_ASSERT_TRUE(xSemaphoreTake(start_sem, pdMS_TO_TICKS(2000)));

    /* send the quit command */
    const char *quit_cmd_line = "quit\n";
    test_send_characters(s_socket_fd[1], quit_cmd_line);

    /* wait for the repl task to signal completion */
    TEST_ASSERT_TRUE(xSemaphoreTake(done_sem, pdMS_TO_TICKS(2000)));

    /* destroy the repl instance */
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_destroy(repl_handle));

    /* cleanup semaphores */
    vSemaphoreDelete(start_sem);
    vSemaphoreDelete(done_sem);

    test_socket_teardown(s_socket_fd);
}
