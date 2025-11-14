/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"

#include "esp_cli.h"
#include "esp_linenoise.h"
#include "esp_cli_commands.h"

#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_vfs_common.h"
#include "driver/esp_private/uart_vfs.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "driver/esp_private/usb_serial_jtag_vfs.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/usb_serial_jtag.h"

static size_t s_on_enter_nb_of_calls = 0;
static size_t s_pre_executor_nb_of_calls = 0;
static size_t s_post_executor_nb_of_calls = 0;
static size_t s_on_stop_nb_of_calls = 0;
static size_t s_on_exit_nb_of_calls = 0;

void test_on_enter(void *ctx, esp_cli_handle_t handle)
{
    s_on_enter_nb_of_calls++;
    return;
}

esp_err_t test_pre_executor(void *ctx, const char *buf, esp_err_t reader_ret_val)
{
    s_pre_executor_nb_of_calls++;
    return ESP_OK;
}

esp_err_t test_post_executor(void *ctx, const char *buf, esp_err_t executor_ret_val, int cmd_ret_val)
{
    s_post_executor_nb_of_calls++;
    return ESP_OK;
}

void test_on_stop(void *ctx, esp_cli_handle_t handle)
{
    s_on_stop_nb_of_calls++;
    return;
}

void test_on_exit(void *ctx, esp_cli_handle_t handle)
{
    s_on_exit_nb_of_calls++;
    return;
}

/* Pass two semaphores:
 *  - start_sem: child gives it when it reached esp_cli() (so main knows child started)
 *  - done_sem:   child gives it just before deleting itself (so main can "join")
 */
typedef struct task_args {
    SemaphoreHandle_t start_sem;
    SemaphoreHandle_t done_sem;
    esp_cli_handle_t hdl;
} task_args_t;

static void esp_cli_task(void *args)
{
    task_args_t *task_args = (task_args_t *)args;

    /* signal to main that task started and esp_cli() will run */
    xSemaphoreGive(task_args->start_sem);

    /* run the esp_cli REPL loop (will return when stopped) */
    esp_cli(task_args->hdl);

    /* signal completion (emulates pthread_join notification) */
    xSemaphoreGive(task_args->done_sem);

    /* self-delete */
    vTaskDelete(NULL);
}

static void test_uart_install(int *in_fd, int *out_fd)
{
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK/XTAL is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
#if SOC_UART_SUPPORT_REF_TICK
    uart_sclk_t clk_source = UART_SCLK_REF_TICK;
    // REF_TICK clock can't provide a high baudrate
    if (CONFIG_ESP_CONSOLE_UART_BAUDRATE > 1 * 1000 * 1000) {
        clk_source = UART_SCLK_DEFAULT;
        ESP_LOGW(TAG, "light sleep UART wakeup might not work at the configured baud rate");
    }
#elif SOC_UART_SUPPORT_XTAL_CLK
    uart_sclk_t clk_source = UART_SCLK_XTAL;
#else
#error "No UART clock source is aware of DFS"
#endif // SOC_UART_SUPPORT_xxx
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = clk_source,
    };

    uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);

    /* Install UART driver for interrupt-driven reads and writes */
    TEST_ASSERT_EQUAL(ESP_OK, uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));

    /* Tell VFS to use UART driver */
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* register the vfs, create a FD used to interface the UART */
    const esp_vfs_fs_ops_t *uart_vfs = esp_vfs_uart_get_vfs();
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_register_fs("/dev/test_uart", uart_vfs, 0, NULL));
    /* open in blocking mode */
    const int uart_fd = open("/dev/test_uart/0", 0);
    TEST_ASSERT(uart_fd != -1);
    *in_fd = uart_fd;
    *out_fd = uart_fd;
}

static void test_uart_uninstall(const int fd)
{
    /* close the stream */
    const int ret = close(fd);
    TEST_ASSERT(ret != -1);

    /* unregister the vfs */
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_unregister("/dev/test_uart"));

    /* uninstall the driver for the default uart */
    uart_vfs_dev_use_nonblocking(CONFIG_ESP_CONSOLE_UART_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, uart_driver_delete(CONFIG_ESP_CONSOLE_UART_NUM));
}

static void test_usj_install(int *in_fd, int *out_fd)
{
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, usb_serial_jtag_driver_install(&usb_serial_jtag_config));

    /* register the vfs, create a FD used to interface the USJ */
    const esp_vfs_fs_ops_t *usj_vfs = esp_vfs_usb_serial_jtag_get_vfs();
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_register_fs("/dev/test_usj", usj_vfs, 0, NULL));
    /* open in blocking mode */
    const int usj_fd = open("/dev/test_usj/0", 0);
    TEST_ASSERT(usj_fd != -1);
    *in_fd = usj_fd;
    *out_fd = usj_fd;
}

static void test_usj_uninstall(const int fd)
{
    /* close the stream */
    const int ret = close(fd);
    TEST_ASSERT(ret != -1);

    /* unregister the vfs */
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_unregister("/dev/test_usj"));

    /* uninstall the driver */
    TEST_ASSERT_EQUAL(ESP_OK, usb_serial_jtag_driver_uninstall());
}

static void test_esp_cli_teardown(SemaphoreHandle_t *start_sem, SemaphoreHandle_t *done_sem,
                                  esp_linenoise_handle_t *linenoise_hdl, esp_cli_handle_t *cli_hdl)
{
    /* destroy the instance of esp_cli */
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_destroy(*cli_hdl));

    /* delete the linenoise instance */
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(*linenoise_hdl));

    /* cleanup semaphores */
    vSemaphoreDelete(*start_sem);
    vSemaphoreDelete(*done_sem);

    s_on_stop_nb_of_calls = 0;
    s_on_exit_nb_of_calls = 0;
    s_on_enter_nb_of_calls = 0;
    s_pre_executor_nb_of_calls = 0;
    s_post_executor_nb_of_calls = 0;
}

static void test_esp_cli_setup(SemaphoreHandle_t *start_sem, SemaphoreHandle_t *done_sem, int in_fd, int out_fd,
                               esp_linenoise_handle_t *linenoise_hdl, esp_cli_handle_t *cli_hdl)
{
    /* create semaphores */
    *start_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(start_sem);
    *done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    /* ensure both semaphores are in the "taken/empty" state:
    taking with 0 timeout guarantees they are empty afterwards
    regardless of the create semantics on this FreeRTOS build. */
    xSemaphoreTake(*start_sem, 0);
    xSemaphoreTake(*done_sem, 0);

    esp_linenoise_config_t linenoise_config;
    esp_linenoise_get_instance_config_default(&linenoise_config);
    linenoise_config.in_fd = in_fd;
    linenoise_config.out_fd = out_fd;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, linenoise_hdl));
    TEST_ASSERT_NOT_NULL(*linenoise_hdl);

    esp_cli_config_t cli_config = {
        .linenoise_handle = *linenoise_hdl,
        .command_set_handle = NULL,
        .max_cmd_line_size = 256,
        .history_save_path = NULL,
        .on_enter = { .func = test_on_enter, .ctx = NULL },
        .pre_executor = { .func = test_pre_executor, .ctx = NULL },
        .post_executor = { .func = test_post_executor, .ctx = NULL },
        .on_stop = { .func = test_on_stop, .ctx = NULL },
        .on_exit = { .func = test_on_exit, .ctx = NULL }
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_create(&cli_config, cli_hdl));
    TEST_ASSERT_NOT_NULL(*cli_hdl);

    s_on_stop_nb_of_calls = 0;
    s_on_exit_nb_of_calls = 0;
    s_on_enter_nb_of_calls = 0;
    s_pre_executor_nb_of_calls = 0;
    s_post_executor_nb_of_calls = 0;
}

TEST_CASE("esp_cli() loop calls callbacks and exit on call to esp_cli_stop", "[esp_cli]")
{
    SemaphoreHandle_t start_sem, done_sem;
    esp_linenoise_handle_t linenoise_hdl;
    esp_cli_handle_t cli_hdl;
    int in_fd, out_fd;

    test_uart_install(&in_fd, &out_fd);
    test_esp_cli_setup(&start_sem, &done_sem, in_fd, out_fd, &linenoise_hdl, &cli_hdl);

    /* create the esp_cli instance task */
    task_args_t args = {.start_sem = start_sem, .done_sem = done_sem, .hdl = cli_hdl};
    BaseType_t rc = xTaskCreate(esp_cli_task, "esp_cli_task", 2048, &args, 5, NULL);
    TEST_ASSERT_EQUAL(pdPASS, rc);

    /* should fail before esp_cli instance is started */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_cli_stop(cli_hdl));

    /* start esp_cli instance */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_cli_start(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_start(cli_hdl));

    /* wait for the esp_cli task to signal it started */
    TEST_ASSERT_TRUE(xSemaphoreTake(start_sem, pdMS_TO_TICKS(2000)));

    /* wait for a bit so esp_cli() has time to loop back into esp_linenoise_get_line */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* stop esp_cli and wait for task to finish (emulate pthread_join) */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_cli_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_stop(cli_hdl));

    /* wait for the esp_cli task to signal completion */
    TEST_ASSERT_TRUE(xSemaphoreTake(done_sem, pdMS_TO_TICKS(2000)));

    /* check that all callbacks were called the right number of times */
    TEST_ASSERT_EQUAL(1, s_on_stop_nb_of_calls);
    TEST_ASSERT_EQUAL(1, s_on_enter_nb_of_calls);
    TEST_ASSERT_EQUAL(1, s_on_exit_nb_of_calls);

    /* make sure calling stop fails because the esp_cli instance is no longer running */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_cli_stop(cli_hdl));

    /* destroy the esp_cli instance */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_cli_destroy(NULL));
    test_esp_cli_teardown(&start_sem, &done_sem, &linenoise_hdl, &cli_hdl);

    /* uninstall the uart driver */
    test_uart_uninstall(in_fd);

    /* make sure the cleanup of the deleted task is done to not bias
     * the memory leak calculations */
    vTaskDelay(pdMS_TO_TICKS(500));
}

TEST_CASE("create and destroy several instances of esp_cli", "[esp_cli]")
{
    /* create semaphores */
    SemaphoreHandle_t start_sem_a, start_sem_b;
    SemaphoreHandle_t done_sem_a, done_sem_b;
    esp_cli_handle_t cli_hdl_a, cli_hdl_b;
    esp_linenoise_handle_t linenoise_hdl_a, linenoise_hdl_b;
    int in_fd_uart, in_fd_usj, out_fd_uart, out_fd_usj;

    /* install uart and usb serial jtag drivers */
    test_uart_install(&in_fd_uart, &out_fd_uart);
    test_usj_install(&in_fd_usj, &out_fd_usj);

    /*  create 2 instances of esp_cli*/
    test_esp_cli_setup(&start_sem_a, &done_sem_a, in_fd_uart, out_fd_uart, &linenoise_hdl_a, &cli_hdl_a);
    test_esp_cli_setup(&start_sem_b, &done_sem_b, in_fd_usj, out_fd_usj, &linenoise_hdl_b, &cli_hdl_b);

    /* create the esp_cli instance task A */
    task_args_t args_a = {.start_sem = start_sem_a, .done_sem = done_sem_a, .hdl = cli_hdl_a};
    BaseType_t rc = xTaskCreate(esp_cli_task, "esp_cli_task_a", 4096, &args_a, 5, NULL);
    TEST_ASSERT_EQUAL(pdPASS, rc);

    /* create the esp_cli instance task B */
    task_args_t args_b = {.start_sem = start_sem_b, .done_sem = done_sem_b, .hdl = cli_hdl_b};
    rc = xTaskCreate(esp_cli_task, "esp_cli_task_b", 4096, &args_b, 5, NULL);
    TEST_ASSERT_EQUAL(pdPASS, rc);

    /* start esp_cli instance */
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_start(cli_hdl_a));
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_start(cli_hdl_b));
    vTaskDelay(pdMS_TO_TICKS(500));

    /* wait for the esp_cli instance tasks to signal it started */
    TEST_ASSERT_TRUE(xSemaphoreTake(start_sem_a, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_TRUE(xSemaphoreTake(start_sem_b, pdMS_TO_TICKS(2000)));

    /* terminate instance A */
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_stop(cli_hdl_a));

    /* wait for the esp_cli instance task to signal completion */
    TEST_ASSERT_TRUE(xSemaphoreTake(done_sem_a, pdMS_TO_TICKS(2000)));

    /* terminate instance B */
    TEST_ASSERT_EQUAL(ESP_OK, esp_cli_stop(cli_hdl_b));

    /* wait for the esp_cli instance task to signal completion */
    TEST_ASSERT_TRUE(xSemaphoreTake(done_sem_b, pdMS_TO_TICKS(2000)));

    test_esp_cli_teardown(&start_sem_a, &done_sem_a, &linenoise_hdl_a, &cli_hdl_a);
    test_esp_cli_teardown(&start_sem_b, &done_sem_b, &linenoise_hdl_b, &cli_hdl_b);

    test_uart_uninstall(in_fd_uart);
    test_usj_uninstall(in_fd_usj);

    /* make sure the cleanup of the deleted task is done to not bias
     * the memory leak calculations */
    vTaskDelay(pdMS_TO_TICKS(500));
}

TEST_CASE("create more esp_linenoise instances that possible based on CONFIG_ESP_LINENOISE_MAX_INSTANCE_NB", "[esp_cli]")
{
    esp_linenoise_config_t linenoise_config;
    esp_linenoise_get_instance_config_default(&linenoise_config);

    const size_t hdl_array_size = CONFIG_ESP_LINENOISE_MAX_INSTANCE_NB + 1;
    esp_linenoise_handle_t hdl_array[hdl_array_size];
    memset(hdl_array, 0x00, sizeof(hdl_array));

    /* try to create more instances than allowed */
    for (size_t i = 0; i < hdl_array_size; i++) {
        if (i < CONFIG_ESP_LINENOISE_MAX_INSTANCE_NB) {
            /* we don't exceed the max number of instance yet, success expected */
            TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, &hdl_array[i]));
            TEST_ASSERT_NOT_NULL(hdl_array[i]);
        } else {
            /* we exceed the max number of instance, failure expected */
            TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, &hdl_array[i]));
            TEST_ASSERT_NULL(hdl_array[i]);
        }
    }

    /* free the instances that were successfully created */\
    for (size_t i = 0; i < hdl_array_size; i++) {
        if (hdl_array[i] != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(hdl_array[i]));
            hdl_array[i] = NULL;
        }
    }

    /* try to create an instance again and deleted */
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&linenoise_config, &hdl_array[0]));
    TEST_ASSERT_NOT_NULL(hdl_array[0]);
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(hdl_array[0]));
}
