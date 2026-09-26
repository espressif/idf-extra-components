/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "xymodem.h"
#include "test_xymodem_common.h"
#include "soc/soc_caps.h"

#if SOC_UART_HP_NUM < 3
#error "Need two extra HP UART ports besides the console to run this test"
#endif

#define XYMODEM_TEST_SUB_PAD         0x1A

typedef struct {
    uint8_t *tx_buf;
    size_t tx_len;
    uint8_t *rx_buf;
    size_t rx_cap;
    size_t rx_len;
    esp_err_t send;
    esp_err_t recv;
} xmodem_loopback_result_t;

/**
 * @note Remember to free the tx_buf and rx_buf in return value.
 */
static xmodem_loopback_result_t xmodem_test_loopback(const xymodem_transport_t *send_transport, const xymodem_transport_t *recv_transport,
                                                     xymodem_check_type_t check_type, xymodem_block_size_t block_size, size_t payload_len)
{
    const size_t rx_cap = payload_len + 1024;
    uint8_t *tx_buf = (uint8_t *)calloc(1, payload_len);
    uint8_t *rx_buf = (uint8_t *)calloc(1, rx_cap);
    if (payload_len > 0) {
        TEST_ASSERT_NOT_NULL(tx_buf);
        xymodem_test_fill_pattern(tx_buf, payload_len);
    }
    TEST_ASSERT_NOT_NULL(rx_buf);

    xymodem_test_mem_source_t source_ctx = { .data = tx_buf, .len = payload_len, .offset = 0 };
    xymodem_test_mem_sink_t sink_ctx = { .data = rx_buf, .cap = rx_cap, .len = 0 };
    const xmodem_data_source_t source = { .read = xymodem_test_read_mem, .ctx = &source_ctx };
    const xmodem_data_sink_t sink = { .write = xymodem_test_write_mem, .ctx = &sink_ctx };

    xymodem_test_result_t res = xymodem_test_run_xmodem(send_transport, recv_transport,
                                                        block_size, check_type, &source, &sink);

    xmodem_loopback_result_t result = {
        .tx_buf = tx_buf,
        .tx_len = payload_len,
        .rx_buf = rx_buf,
        .rx_cap = rx_cap,
        .rx_len = sink_ctx.len,
        .send = res.send,
        .recv = res.recv,
    };
    return result;
}

static void xmodem_test_loopback_verify(const xmodem_loopback_result_t *result)
{
    TEST_ASSERT_EQUAL(ESP_OK, result->send);
    TEST_ASSERT_EQUAL(ESP_OK, result->recv);

    TEST_ASSERT_TRUE_MESSAGE(result->rx_len >= result->tx_len, "received fewer bytes than sent");
    // verify the payload
    if (result->tx_len > 0) {
        TEST_ASSERT_EQUAL_UINT8_ARRAY(result->tx_buf, result->rx_buf, result->tx_len);
    }
    // verify the padding
    for (size_t i = result->tx_len; i < result->rx_len; i++) {
        TEST_ASSERT_EQUAL_HEX8(XYMODEM_TEST_SUB_PAD, result->rx_buf[i]);
    }
}

/**************************
 * Normal test
 **************************/

static xymodem_test_uart_ctx_t s_send_ctx = { .port = XYMODEM_TEST_SENDER_UART };
static xymodem_test_uart_ctx_t s_recv_ctx = { .port = XYMODEM_TEST_RECEIVER_UART };

static xymodem_transport_t s_send_transport_normal = {
    .recv = xymodem_test_recv_uart,
    .send = xymodem_test_send_uart,
    .ctx = &s_send_ctx,
};
static xymodem_transport_t s_recv_transport_normal = {
    .recv = xymodem_test_recv_uart,
    .send = xymodem_test_send_uart,
    .ctx = &s_recv_ctx,
};

TEST_CASE("XMODEM empty payload", "[xmodem]")
{
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 0);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 0);  // should not receive anything
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM CRC16 128-byte single packet", "[xmodem]")
{
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 64);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 128);  // should receive 1 packet, 128 bytes
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM CRC16 128-byte multiple packets", "[xmodem]")
{
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 300);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 128 * 3);  // should receive 3 packets, 128 bytes each
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM CRC16 1K mixed blocks", "[xmodem]")
{
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_MIXED, 2049);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 2 * 1024 + 128);  // should receive 3 packets, 1024, 1024 and 128 bytes
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM SUM8 128-byte multiple packets", "[xmodem]")
{
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_SUM8, XYMODEM_BLOCK_SIZE_128, 256);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 128 * 2);  // should receive 2 packets, 128 bytes each
    free(result.tx_buf);
    free(result.rx_buf);
}

/**************************
 * Drop packet test
 **************************/

static uint8_t s_drop_char;
static uint32_t s_drop_count = 0;

/**
 * Drop specified number of packets with the specified character.
 */
static esp_err_t xymodem_test_send_drop(void *ctx, const void *buf, size_t len)
{
    if (len == 1 && *(const uint8_t *)buf == s_drop_char && s_drop_count > 0) {
        s_drop_count--;
        return ESP_OK;
    }
    return xymodem_test_send_uart(ctx, buf, len);
}

static const xymodem_transport_t s_recv_transport_drop = {
    .recv = xymodem_test_recv_uart,
    .send = xymodem_test_send_drop,
    .ctx = &s_recv_ctx,
};

TEST_CASE("XMODEM CRC16 fallback to SUM8", "[xmodem]")
{
    s_drop_char = 'C';
    s_drop_count = XYMODEM_TEST_MAX_RETRY_CNT + 1;
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_drop,
                                                           XYMODEM_CHECK_AUTO, XYMODEM_BLOCK_SIZE_128, 1);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 128);  // should receive 1 packet, 128 bytes
    TEST_ASSERT_EQUAL(s_drop_count, 0);  // should have dropped MAX_RETRY_CNT + 1 CRC packets
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM CRC16 fallback failed", "[xmodem]")
{
    s_drop_char = 'C';
    s_drop_count = XYMODEM_TEST_MAX_RETRY_CNT + 1;
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_drop,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 1);
    TEST_ASSERT_EQUAL(s_drop_count, 0);  // should have dropped MAX_RETRY_CNT + 1 CRC packets
    TEST_ASSERT_EQUAL(result.recv, ESP_ERR_XYMODEM_NO_RESPONSE);
    TEST_ASSERT_EQUAL(result.send, ESP_ERR_XYMODEM_NO_RESPONSE);
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM CRC16 handshake retry MAX_RETRY_CNT times", "[xmodem]")
{
    s_drop_char = 'C';
    s_drop_count = XYMODEM_TEST_MAX_RETRY_CNT;
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_drop,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 1);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 128);  // should receive 1 packet, 128 bytes
    TEST_ASSERT_EQUAL(s_drop_count, 0);  // should have dropped MAX_RETRY_CNT CRC packets
    free(result.tx_buf);
    free(result.rx_buf);
}

TEST_CASE("XMODEM Lost ACK", "[xmodem]")
{
    s_drop_char = 0x06;  // ACK
    s_drop_count = 2;
    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_normal, &s_recv_transport_drop,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 256);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);  // should receive 2 packets, 128 bytes each
    TEST_ASSERT_EQUAL(s_drop_count, 0);  // should have dropped 2 ACK packets
    free(result.tx_buf);
    free(result.rx_buf);
}

/**************************
 * Flip bit test
 **************************/

typedef struct {
    uint8_t soh_stx;
    uint8_t blk;
    uint8_t blk_inv;
    uint8_t data[];
} xmodem_packet_header_t;

static void* s_flip_bit_workspace;
static enum {
    FLIP_HEADER,
    FLIP_BLK,
    FLIP_DATA,
} s_flip_bit_where;
static uint32_t s_flip_bit_target_blk;

static esp_err_t xymodem_test_send_flip_bit(void *ctx, const void *buf, size_t len)
{
    if (len > 128 && s_flip_bit_workspace == NULL &&
            ((xmodem_packet_header_t *)buf)->blk == s_flip_bit_target_blk) {
        s_flip_bit_workspace = malloc(len);
        TEST_ASSERT_NOT_NULL(s_flip_bit_workspace);
        memcpy(s_flip_bit_workspace, buf, len);

        xmodem_packet_header_t *header = s_flip_bit_workspace;
        switch (s_flip_bit_where) {
        case FLIP_HEADER:
            header->soh_stx ^= 0x20;
            break;
        case FLIP_BLK:
            header->blk ^= 0x20;
            break;
        case FLIP_DATA:
            header->data[3] ^= 0x20;
            break;
        }
        return xymodem_test_send_uart(ctx, s_flip_bit_workspace, len);
    }
    return xymodem_test_send_uart(ctx, buf, len);
}

static const xymodem_transport_t s_send_transport_flip_bit = {
    .recv = xymodem_test_recv_uart,
    .send = xymodem_test_send_flip_bit,
    .ctx = &s_send_ctx,
};

TEST_CASE("XMODEM flip bit in header, blk=1", "[xmodem]")
{
    s_flip_bit_where = FLIP_HEADER;
    s_flip_bit_target_blk = 1;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

TEST_CASE("XMODEM flip bit in blk, blk=1", "[xmodem]")
{
    s_flip_bit_where = FLIP_BLK;
    s_flip_bit_target_blk = 1;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

TEST_CASE("XMODEM flip bit in data, blk=1", "[xmodem]")
{
    s_flip_bit_where = FLIP_DATA;
    s_flip_bit_target_blk = 1;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

TEST_CASE("XMODEM flip bit in header, blk=2", "[xmodem]")
{
    s_flip_bit_where = FLIP_HEADER;
    s_flip_bit_target_blk = 2;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

TEST_CASE("XMODEM flip bit in blk, blk=2", "[xmodem]")
{
    s_flip_bit_where = FLIP_BLK;
    s_flip_bit_target_blk = 2;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

TEST_CASE("XMODEM flip bit in data, blk=2", "[xmodem]")
{
    s_flip_bit_where = FLIP_DATA;
    s_flip_bit_target_blk = 2;

    xmodem_loopback_result_t result = xmodem_test_loopback(&s_send_transport_flip_bit, &s_recv_transport_normal,
                                                           XYMODEM_CHECK_CRC16, XYMODEM_BLOCK_SIZE_128, 180);
    xmodem_test_loopback_verify(&result);
    TEST_ASSERT_EQUAL(result.rx_len, 256);
    free(result.tx_buf);
    free(result.rx_buf);

    TEST_ASSERT_TRUE(s_flip_bit_workspace != NULL);
    free(s_flip_bit_workspace);
    s_flip_bit_workspace = NULL;
}

/**************************
 * Convenience APIs test
 **************************/

TEST_CASE("XMODEM send/recv buffer", "[xmodem]")
{
    const size_t payload_len = 180;
    const size_t rx_cap = 256;
    uint8_t *tx_buf = (uint8_t *)calloc(1, payload_len);
    uint8_t *rx_buf = (uint8_t *)calloc(1, rx_cap);
    TEST_ASSERT_NOT_NULL(tx_buf);
    TEST_ASSERT_NOT_NULL(rx_buf);
    xymodem_test_fill_pattern(tx_buf, payload_len);

    size_t recv_len = 0;
    xymodem_test_result_t res = xymodem_test_run_xmodem_buffer(&s_send_transport_normal, &s_recv_transport_normal,
                                                               XYMODEM_BLOCK_SIZE_128, XYMODEM_CHECK_CRC16,
                                                               tx_buf, payload_len, rx_buf, rx_cap, &recv_len);
    TEST_ASSERT_EQUAL(ESP_OK, res.send);
    TEST_ASSERT_EQUAL(ESP_OK, res.recv);
    TEST_ASSERT_EQUAL(256, recv_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx_buf, rx_buf, payload_len);
    for (size_t i = payload_len; i < recv_len; i++) {
        TEST_ASSERT_EQUAL_HEX8(XYMODEM_TEST_SUB_PAD, rx_buf[i]);
    }

    free(tx_buf);
    free(rx_buf);
}

TEST_CASE("XMODEM recv buffer full aborts", "[xmodem]")
{
    // Two full 128-byte packets. The first write fills rx_buf; the second must abort.
    const size_t payload_len = 256;
    const size_t rx_cap = 128;
    uint8_t *tx_buf = (uint8_t *)calloc(1, payload_len);
    uint8_t *rx_buf = (uint8_t *)calloc(1, rx_cap);
    TEST_ASSERT_NOT_NULL(tx_buf);
    TEST_ASSERT_NOT_NULL(rx_buf);
    xymodem_test_fill_pattern(tx_buf, payload_len);

    size_t recv_len = 0;
    xymodem_test_result_t res = xymodem_test_run_xmodem_buffer(&s_send_transport_normal, &s_recv_transport_normal,
                                                               XYMODEM_BLOCK_SIZE_128, XYMODEM_CHECK_CRC16,
                                                               tx_buf, payload_len, rx_buf, rx_cap, &recv_len);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, res.recv);
    TEST_ASSERT_EQUAL(ESP_ERR_XYMODEM_CANCEL_BY_PEER, res.send);
    TEST_ASSERT_EQUAL(rx_cap, recv_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx_buf, rx_buf, rx_cap);

    free(tx_buf);
    free(rx_buf);
}
