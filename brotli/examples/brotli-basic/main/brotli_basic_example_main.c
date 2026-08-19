/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "esp_log.h"
#include "brotli/decode.h"
#include "brotli/encode.h"

static const char *TAG = "brotli-basic";

#define EXAMPLE_QUALITY  1
#define EXAMPLE_LGWIN    16

static const uint8_t input[] =
    "Brotli is a lossless compression algorithm. "
    "This example compresses this in-memory buffer, decompresses it, "
    "and verifies that the original data is recovered.";

void app_main(void)
{
    const size_t input_size = sizeof(input) - 1;
    size_t compressed_capacity = BrotliEncoderMaxCompressedSize(input_size);
    uint8_t *compressed = malloc(compressed_capacity);
    uint8_t *decoded = malloc(input_size);

    ESP_LOGI(TAG, "Compressing %zu bytes with a %u KiB window",
             input_size, 1U << (EXAMPLE_LGWIN - 10));

    if (compressed == NULL || decoded == NULL) {
        ESP_LOGE(TAG, "Failed to allocate compression buffers");
        abort();
    }

    size_t compressed_size = compressed_capacity;
    if (!BrotliEncoderCompress(EXAMPLE_QUALITY, EXAMPLE_LGWIN,
                               BROTLI_MODE_GENERIC,
                               input_size, input,
                               &compressed_size, compressed)) {
        ESP_LOGE(TAG, "Compression failed; increase available heap if needed");
        abort();
    }

    size_t decoded_size = input_size;
    BrotliDecoderResult result = BrotliDecoderDecompress(compressed_size,
                                                         compressed,
                                                         &decoded_size,
                                                         decoded);
    if (result != BROTLI_DECODER_RESULT_SUCCESS ||
            decoded_size != input_size ||
            memcmp(input, decoded, input_size) != 0) {
        ESP_LOGE(TAG, "Round-trip verification failed");
        abort();
    }

    ESP_LOGI(TAG, "Compressed %zu bytes to %zu bytes",
             input_size, compressed_size);
    ESP_LOGI(TAG, "Brotli round-trip verified");

    free(decoded);
    free(compressed);
}
