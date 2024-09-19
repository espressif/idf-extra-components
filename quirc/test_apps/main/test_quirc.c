/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "quirc.h"
#include "unity.h"

static const char *TAG = "test_quirc";

extern const uint8_t test_qrcode_pgm_start[] asm("_binary_test_qrcode_pgm_start");
extern const uint8_t test_qrcode_pgm_end[]   asm("_binary_test_qrcode_pgm_end");

static void copy_test_image_into_quirc_buffer(struct quirc *q)
{
    // get the size of the image from the PGM header
    const uint8_t *p = test_qrcode_pgm_start;
    int width, height;
    sscanf((const char *)p, "P5 %d %d 255", &width, &height);
    TEST_ASSERT_EQUAL_INT(128, width);
    TEST_ASSERT_EQUAL_INT(113, height);

    // resize the quirc buffer to match the image
    TEST_ASSERT_EQUAL_INT(0, quirc_resize(q, width, height));

    // find the start of the image data
    p = memchr(p, '\n', test_qrcode_pgm_end - p) + 1;

    // copy the image into the quirc buffer
    memcpy(quirc_begin(q, NULL, NULL), p, width * height);
}

typedef struct {
    struct quirc *q;
    struct quirc_code code;
    struct quirc_data data;
    SemaphoreHandle_t done;
} quirc_decode_task_args_t;

static void quirc_decode_task(void *arg)
{
    quirc_decode_task_args_t *args = (quirc_decode_task_args_t *)arg;
    quirc_end(args->q);
    TEST_ASSERT_EQUAL_INT(1, quirc_count(args->q));
    quirc_extract(args->q, 0, &args->code);
    TEST_ASSERT_EQUAL(QUIRC_SUCCESS, quirc_decode(&args->code, &args->data));

    const size_t stack_space_free = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "quirc_decode_task stack space free: %d", stack_space_free);
    xSemaphoreGive(args->done);
    vTaskDelete(NULL);
}

TEST_CASE("quirc can load a QR code", "[quirc]")
{
    struct quirc *q = quirc_new();
    TEST_ASSERT_NOT_NULL(q);

    // load the test image into the quirc buffer
    copy_test_image_into_quirc_buffer(q);

    // decode the QR code in the image
    // quirc uses a lot of stack space (around 10kB on ESP32 for this particular QR code),
    // so do this in a separate task
    quirc_decode_task_args_t *args = calloc(1, sizeof(*args));
    TEST_ASSERT_NOT_NULL(args);
    args->q = q;
    args->done = xSemaphoreCreateBinary();
    TEST_ASSERT(xTaskCreate(quirc_decode_task, "quirc_decode_task", 12000, args, 5, NULL));
    TEST_ASSERT(xSemaphoreTake(args->done, pdMS_TO_TICKS(10000)));
    vSemaphoreDelete(args->done);

    // check the QR code data
    TEST_ASSERT_EQUAL_INT(1, args->data.version);
    TEST_ASSERT_EQUAL_INT(1, args->data.ecc_level);
    TEST_ASSERT_EQUAL_INT(4, args->data.data_type);
    TEST_ASSERT_EQUAL_INT(13, args->data.payload_len);
    TEST_ASSERT_EQUAL_STRING("test of quirc", args->data.payload);

    free(args);
    quirc_destroy(q);
    vTaskDelay(2);  // allow the task to clean up
}
