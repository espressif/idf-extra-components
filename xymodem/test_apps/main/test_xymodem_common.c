/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "unity.h"
#include "test_xymodem_common.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_BUF_SIZE        2048
#define GPIO_A               CONFIG_XYMODEM_TEST_GPIO_A
#define GPIO_B               CONFIG_XYMODEM_TEST_GPIO_B
#define UART_BAUD            CONFIG_XYMODEM_TEST_UART_BAUD

#define TRANSFER_TASK_STACK  6144

void xymodem_test_fill_pattern(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(i * 7u + 3u);
    }
}

/**************************************
 * UART transport
 **************************************/

void xymodem_test_uart_init(void)
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    TEST_ASSERT_NOT_EQUAL_MESSAGE(XYMODEM_TEST_SENDER_UART, XYMODEM_TEST_RECEIVER_UART,
                                  "sender and receiver UART must differ");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(GPIO_A, GPIO_B, "loopback GPIOs must differ");
    TEST_ASSERT_LESS_THAN_MESSAGE(SOC_UART_NUM, XYMODEM_TEST_SENDER_UART,
                                  "sender UART port is not available on this target");
    TEST_ASSERT_LESS_THAN_MESSAGE(SOC_UART_NUM, XYMODEM_TEST_RECEIVER_UART,
                                  "receiver UART port is not available on this target");

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    TEST_ASSERT_EQUAL(ESP_OK, uart_driver_install(XYMODEM_TEST_SENDER_UART, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    TEST_ASSERT_EQUAL(ESP_OK, uart_param_config(XYMODEM_TEST_SENDER_UART, &uart_config));
    TEST_ASSERT_EQUAL(ESP_OK, uart_driver_install(XYMODEM_TEST_RECEIVER_UART, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    TEST_ASSERT_EQUAL(ESP_OK, uart_param_config(XYMODEM_TEST_RECEIVER_UART, &uart_config));

    /*
     * Share two GPIOs so that:
     *   GPIO_A: sender TX + receiver RX
     *   GPIO_B: receiver TX + sender RX
     */
    TEST_ASSERT_EQUAL(ESP_OK, uart_set_pin(XYMODEM_TEST_SENDER_UART, GPIO_A, GPIO_B,
                                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    TEST_ASSERT_EQUAL(ESP_OK, uart_set_pin(XYMODEM_TEST_RECEIVER_UART, GPIO_B, GPIO_A,
                                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(ESP_OK, uart_flush_input(XYMODEM_TEST_SENDER_UART));
    TEST_ASSERT_EQUAL(ESP_OK, uart_flush_input(XYMODEM_TEST_RECEIVER_UART));
}

esp_err_t xymodem_test_recv_uart(void *ctx, void *buf, size_t len, size_t *recv_len, uint32_t timeout_ms)
{
    xymodem_test_uart_ctx_t *uart = (xymodem_test_uart_ctx_t *)ctx;
    uint32_t ticks_to_wait = pdMS_TO_TICKS(timeout_ms) ? : (timeout_ms ? 1 : 0);
    int n = uart_read_bytes(uart->port, buf, len, ticks_to_wait);
    if (n < 0) {
        return ESP_FAIL;
    }
    *recv_len = (size_t)n;
    return ESP_OK;
}

esp_err_t xymodem_test_send_uart(void *ctx, const void *buf, size_t len)
{
    xymodem_test_uart_ctx_t *uart = (xymodem_test_uart_ctx_t *)ctx;
    int n = uart_write_bytes(uart->port, buf, len);
    if (n < 0 || (size_t)n != len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**************************************
 * Memory source and sink
 **************************************/

esp_err_t xymodem_test_read_mem(void *ctx, void *buf, size_t len, size_t *read_len)
{
    xymodem_test_mem_source_t *src = (xymodem_test_mem_source_t *)ctx;
    size_t remain = src->len - src->offset;
    size_t n = remain < len ? remain : len;
    if (n > 0) {
        memcpy(buf, src->data + src->offset, n);
        src->offset += n;
    }
    *read_len = n;
    return ESP_OK;
}

esp_err_t xymodem_test_write_mem(void *ctx, const void *buf, size_t len)
{
    xymodem_test_mem_sink_t *sink = (xymodem_test_mem_sink_t *)ctx;
    if (sink->len + len > sink->cap) {
        return ESP_ERR_NO_MEM;
    }
    if (len > 0) {
        memcpy(sink->data + sink->len, buf, len);
        sink->len += len;
    }
    return ESP_OK;
}

/**************************************
 * Two tasks runner
 **************************************/

/**
 * Run a sender task and a receiver task, wait until both have notified the
 * caller, then delete them.
 *
 * Capture `xTaskGetCurrentTaskHandle()` before calling and store it in both
 * job arguments. Each task must `xTaskNotifyGive()` that handle when finished
 * and then `vTaskSuspend()`.
 */
static void xymodem_test_run_sender_receiver(void (*sender)(void *), void *sender_arg,
                                             void (*receiver)(void *), void *receiver_arg)
{
    static StackType_t s_send_stack[TRANSFER_TASK_STACK / sizeof(StackType_t)];
    static StackType_t s_recv_stack[TRANSFER_TASK_STACK / sizeof(StackType_t)];
    static StaticTask_t s_send_tcb;
    static StaticTask_t s_recv_tcb;

    while (ulTaskNotifyTake(pdTRUE, 0) != 0) {
    }

    TaskHandle_t sender_handle = xTaskCreateStatic(sender, "xymodem_tx", sizeof(s_send_stack),
                                                   sender_arg, 5, s_send_stack, &s_send_tcb);
    TEST_ASSERT_NOT_NULL_MESSAGE(sender_handle, "failed to create sender task");

    TaskHandle_t receiver_handle = xTaskCreateStatic(receiver, "xymodem_rx", sizeof(s_recv_stack),
                                                     receiver_arg, 5, s_recv_stack, &s_recv_tcb);
    TEST_ASSERT_NOT_NULL_MESSAGE(receiver_handle, "failed to create receiver task");

    for (int i = 0; i < 2; i++) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    vTaskDelete(sender_handle);
    vTaskDelete(receiver_handle);
}

/**************************************
 * XMODEM send and receive
 **************************************/

static xymodem_send_config_t xymodem_test_send_config(xymodem_block_size_t block_size)
{
    xymodem_send_config_t config = XYMODEM_SEND_CONFIG_DEFAULT();
    config.response_timeout_ms = XYMODEM_TEST_SEND_TIMEOUT_MS;
    config.max_retry_cnt = XYMODEM_TEST_MAX_RETRY_CNT;
    config.block_size = block_size;
    return config;
}

static xymodem_recv_config_t xymodem_test_recv_config(xymodem_check_type_t check_type)
{
    xymodem_recv_config_t config = XYMODEM_RECV_CONFIG_DEFAULT();
    config.packet_timeout_ms = XYMODEM_TEST_RECV_TIMEOUT_MS;
    config.max_retry_cnt = XYMODEM_TEST_MAX_RETRY_CNT;
    config.check_type = check_type;
    return config;
}

typedef struct {
    const xymodem_transport_t *transport;
    const xymodem_send_config_t *config;
    const xmodem_data_source_t *source;
    esp_err_t result;
    TaskHandle_t notify_task;
} send_job_t;

typedef struct {
    const xymodem_transport_t *transport;
    const xymodem_recv_config_t *config;
    const xmodem_data_sink_t *sink;
    esp_err_t result;
    TaskHandle_t notify_task;
} recv_job_t;

static void send_task(void *arg)
{
    send_job_t *job = (send_job_t *)arg;
    job->result = xmodem_send(job->transport, job->config, job->source);
    xTaskNotifyGive(job->notify_task);
    vTaskSuspend(NULL);
}

static void recv_task(void *arg)
{
    recv_job_t *job = (recv_job_t *)arg;
    job->result = xmodem_recv(job->transport, job->config, job->sink);
    xTaskNotifyGive(job->notify_task);
    vTaskSuspend(NULL);
}

xymodem_test_result_t xymodem_test_run_xmodem(const xymodem_transport_t *send_transport,
                                              const xymodem_transport_t *recv_transport,
                                              xymodem_block_size_t block_size,
                                              xymodem_check_type_t check_type,
                                              const xmodem_data_source_t *source,
                                              const xmodem_data_sink_t *sink)
{
    const xymodem_send_config_t send_config = xymodem_test_send_config(block_size);
    const xymodem_recv_config_t recv_config = xymodem_test_recv_config(check_type);

    TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
    send_job_t send_job = {
        .transport = send_transport,
        .config = &send_config,
        .source = source,
        .result = ESP_ERR_TIMEOUT,
        .notify_task = waiter,
    };
    recv_job_t recv_job = {
        .transport = recv_transport,
        .config = &recv_config,
        .sink = sink,
        .result = ESP_ERR_TIMEOUT,
        .notify_task = waiter,
    };

    xymodem_test_run_sender_receiver(send_task, &send_job, recv_task, &recv_job);

    return (xymodem_test_result_t) {
        .send = send_job.result,
        .recv = recv_job.result,
    };
}

/**************************************
 * XMODEM send and receive (buffer)
 **************************************/

typedef struct {
    const xymodem_transport_t *transport;
    const xymodem_send_config_t *config;
    const void *buf;
    size_t len;
    esp_err_t result;
    TaskHandle_t notify_task;
} send_buffer_job_t;

typedef struct {
    const xymodem_transport_t *transport;
    const xymodem_recv_config_t *config;
    void *buf;
    size_t cap;
    size_t copied;
    esp_err_t result;
    TaskHandle_t notify_task;
} recv_buffer_job_t;

static void send_buffer_task(void *arg)
{
    send_buffer_job_t *job = (send_buffer_job_t *)arg;
    job->result = xmodem_send_buffer(job->transport, job->config, job->buf, job->len);
    xTaskNotifyGive(job->notify_task);
    vTaskSuspend(NULL);
}

static void recv_buffer_task(void *arg)
{
    recv_buffer_job_t *job = (recv_buffer_job_t *)arg;
    job->result = xmodem_recv_buffer(job->transport, job->config, job->buf, job->cap, &job->copied);
    xTaskNotifyGive(job->notify_task);
    vTaskSuspend(NULL);
}

xymodem_test_result_t xymodem_test_run_xmodem_buffer(const xymodem_transport_t *send_transport,
                                                     const xymodem_transport_t *recv_transport,
                                                     xymodem_block_size_t block_size,
                                                     xymodem_check_type_t check_type,
                                                     const void *tx_buf, size_t tx_len,
                                                     void *rx_buf, size_t rx_cap,
                                                     size_t *recv_len)
{
    const xymodem_send_config_t send_config = xymodem_test_send_config(block_size);
    const xymodem_recv_config_t recv_config = xymodem_test_recv_config(check_type);

    TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
    send_buffer_job_t send_job = {
        .transport = send_transport,
        .config = &send_config,
        .buf = tx_buf,
        .len = tx_len,
        .result = ESP_ERR_TIMEOUT,
        .notify_task = waiter,
    };
    recv_buffer_job_t recv_job = {
        .transport = recv_transport,
        .config = &recv_config,
        .buf = rx_buf,
        .cap = rx_cap,
        .copied = 0,
        .result = ESP_ERR_TIMEOUT,
        .notify_task = waiter,
    };

    xymodem_test_run_sender_receiver(send_buffer_task, &send_job, recv_buffer_task, &recv_job);

    if (recv_len) {
        *recv_len = recv_job.copied;
    }
    return (xymodem_test_result_t) {
        .send = send_job.result,
        .recv = recv_job.result,
    };
}
