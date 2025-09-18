/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "esp_repl.h"
#include "esp_linenoise.h"
#include "esp_commands.h"

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
}

static void test_socket_teardown(int socket_fd[2])
{
    close(socket_fd[0]);
    close(socket_fd[1]);
}

static void test_send_characters(int socket_fd, const char *msg)
{
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

void test_on_stop(void *ctx, esp_repl_instance_handle_t handle)
{
    s_on_stop_nb_of_calls++;
    return;
}

void test_on_exit(void *ctx, esp_repl_instance_handle_t handle)
{
    s_on_exit_nb_of_calls++;
    return;
}

typedef struct task_args {
    pthread_mutex_t *lock;
    esp_repl_instance_handle_t hdl;
} task_args_t;

static void *repl_task(void *args)
{
    /* wait for a bit so esp_linenoise has time to properly initialize */
    vTaskDelay(pdMS_TO_TICKS(500));

    task_args_t *task_args = (task_args_t *)args;

    pthread_mutex_unlock(task_args->lock);

    esp_repl(task_args->hdl);

    return NULL;
}

TEST_CASE("esp_repl() loop calls all callbacks and exit on call to esp_repl_stop", "[esp_repl]")
{
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);

    esp_linenoise_config_t linenoise_config;
    esp_linenoise_get_instance_config_default(&linenoise_config);
    test_socket_setup(s_socket_fd);
    esp_linenoise_handle_t esp_linenoise_hdl = esp_linenoise_create_instance(&linenoise_config);

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

    esp_repl_instance_handle_t repl_handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_create(&repl_handle, &repl_config));
    TEST_ASSERT_NOT_NULL(repl_handle);

    task_args_t args = {.lock = &lock, .hdl = repl_handle};

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, repl_task, &args);

    /* should fail before repl is started */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    /* start repl */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_start(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_start(repl_handle));

    /* wait for the esp_repl to be reached */
    pthread_mutex_lock(&lock);

    /* send a dummy string new line terminated to trigger linenoise to return */
    const char *input_line = "dummy_message\n";
    test_send_characters(s_socket_fd[1], input_line);

    /* wait for a bit so so esp_repl() has time to loop back  into esp_linenoise_get_line */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* check that pre-executor, post-executor callbacks are called */
    TEST_ASSERT_EQUAL(1, s_pre_executor_nb_of_calls);
    TEST_ASSERT_EQUAL(1, s_post_executor_nb_of_calls);

    /* here, esp_repl() should be back in esp_linenoise_get_line, call
     * esp_repl_stop() and check that both the on_stop and on_exit callbacks
     * are being called */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));

    /* wait for the task to return to make sure the esp_repl instance got stoped */
    pthread_join(thread_id, NULL);

    /* check that all callbacks where called the right number of times */
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

    /* should fail after the instance is destroy */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_start(repl_handle));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_stop(repl_handle));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_repl_destroy(repl_handle));

    pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);

    test_socket_teardown(s_socket_fd);
}
