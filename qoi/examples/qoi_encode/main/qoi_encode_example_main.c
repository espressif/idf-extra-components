/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#include "qoi.h"

#define EXAMPLE_WIDTH             160
#define EXAMPLE_HEIGHT            120
#define EXAMPLE_CHANNELS          4  /* RGBA */
#define EXAMPLE_FRAME_SIZE        (EXAMPLE_WIDTH * EXAMPLE_HEIGHT * EXAMPLE_CHANNELS)
#define EXAMPLE_BASE64_CHUNK_LEN  96

/* Generate a synthetic RGBA framebuffer on the CPU. This mimics the pixel
 * content an LCD "screenshot" would read out of a display framebuffer. The
 * pattern is chosen to contain both smooth gradients (compress well) and
 * high-frequency details (stress the encoder), plus a small colored icon so
 * the reconstructed image on the host is easy to inspect. */
static void example_fill_rgba(uint8_t *px, uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t i = (y * width + x) * 4;
            /* Soft gradient background. */
            px[i + 0] = (uint8_t)(x * 255 / width);         /* R */
            px[i + 1] = (uint8_t)(y * 255 / height);        /* G */
            px[i + 2] = (uint8_t)((x + y) * 255 / (width + height)); /* B */
            px[i + 3] = 0xff;                               /* A */
            /* Draw a few solid rectangles (long runs compress very well). */
            if (x >= width / 4 && x < 3 * width / 4 &&
                    y >= height / 4 && y < 3 * height / 4) {
                px[i + 0] = 0xE0;
                px[i + 1] = 0x30;
                px[i + 2] = 0x40;
            }
        }
    }
}

/* Print the base64 payload split into short lines so that it is easy to read
 * in the serial monitor and robust for pytest to parse back into a .qoi. */
static void print_base64_payload(const unsigned char *encoded, size_t encoded_len)
{
    printf("QOI_BASE64_BEGIN\n");
    for (size_t offset = 0, chunk_index = 0; offset < encoded_len;
            offset += EXAMPLE_BASE64_CHUNK_LEN, chunk_index++) {
        size_t chunk_len = encoded_len - offset;
        if (chunk_len > EXAMPLE_BASE64_CHUNK_LEN) {
            chunk_len = EXAMPLE_BASE64_CHUNK_LEN;
        }
        printf("QOI_BASE64 %.*s\n", (int)chunk_len, (const char *)&encoded[offset]);
        if ((chunk_index + 1) % 16 == 0) {
            vTaskDelay(1);
        }
    }
    printf("QOI_BASE64_END\n");
}

void app_main(void)
{
    /* Raw RGBA framebuffer generated on the CPU. */
    uint8_t *rgba = malloc(EXAMPLE_FRAME_SIZE);
    ESP_ERROR_CHECK(rgba != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    example_fill_rgba(rgba, EXAMPLE_WIDTH, EXAMPLE_HEIGHT);
    printf("Generated %" PRIu32 "x%" PRIu32 " RGBA framebuffer: %zu bytes\n",
           (uint32_t)EXAMPLE_WIDTH, (uint32_t)EXAMPLE_HEIGHT, (size_t)EXAMPLE_FRAME_SIZE);

    /* Encode the in-memory RGBA buffer into a QOI bitstream in memory. */
    qoi_desc desc = {
        .width = EXAMPLE_WIDTH,
        .height = EXAMPLE_HEIGHT,
        .channels = EXAMPLE_CHANNELS,
        .colorspace = QOI_SRGB,
    };
    int qoi_size = 0;
    uint8_t *qoi_buf = (uint8_t *)qoi_encode(rgba, &desc, &qoi_size);
    ESP_ERROR_CHECK(qoi_buf != NULL ? ESP_OK : ESP_FAIL);

    printf("Encoded QOI size: %d bytes (ratio %.2f%%)\n",
           qoi_size, 100.0f * qoi_size / EXAMPLE_FRAME_SIZE);

    /* base64-encode the QOI bitstream so it can be sent over the serial
     * console as plain text and reassembled on the host by pytest. */
    size_t encoded_len = 0;
    int ret = mbedtls_base64_encode(NULL, 0, &encoded_len, qoi_buf, qoi_size);
    ESP_ERROR_CHECK((ret == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) ? ESP_OK : ESP_FAIL);
    unsigned char *encoded = calloc(encoded_len + 1, 1);
    ESP_ERROR_CHECK(encoded != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(mbedtls_base64_encode(encoded, encoded_len + 1, &encoded_len, qoi_buf, qoi_size) == 0 ? ESP_OK : ESP_FAIL);

    /* QOI_META plus the chunked QOI_BASE64 lines form a tiny text protocol
     * that pytest understands and can reconstruct into a host-side .qoi file. */
    printf("QOI_META width=%" PRIu32 " height=%" PRIu32 " channels=%" PRIu32
           " colorspace=%" PRIu32 " encoding=base64 size=%d\n",
           (uint32_t)desc.width, (uint32_t)desc.height, (uint32_t)desc.channels,
           (uint32_t)desc.colorspace, qoi_size);
    print_base64_payload(encoded, encoded_len);
    printf("QOI encode demo done.\n");

    free(encoded);
    free(qoi_buf);
    free(rgba);
}
