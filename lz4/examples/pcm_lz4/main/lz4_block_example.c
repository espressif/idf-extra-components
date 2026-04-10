/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "lz4.h"

#include "lz4_example.h"

#define LZ4_CHECK(condition) ESP_ERROR_CHECK((condition) ? ESP_OK : ESP_FAIL)

static void fill_pcm_like_samples(int16_t *samples, size_t sample_count)
{
    uint32_t state = 0xC0FFEE01u;
    int32_t current = 0;

    for (size_t i = 0; i < sample_count; i++) {
        state = (state * 1664525u) + 1013904223u;
        const int32_t saw = (int32_t)(i % 128u) - 64;
        const int32_t noise = (int32_t)((state >> 28) & 0x0Fu) - 8;
        const int32_t target = saw * 256;

        current += (target - current) / 8;
        current += noise;
        if (current > INT16_MAX) {
            current = INT16_MAX;
        } else if (current < INT16_MIN) {
            current = INT16_MIN;
        }
        samples[i] = (int16_t)current;
    }
}

void lz4_run_block_example(void)
{
    enum { SAMPLE_COUNT = 4096 };
    const size_t source_size = SAMPLE_COUNT * sizeof(int16_t);

    printf("\nBlock API example\n");

    uint8_t *source = malloc(source_size);
    LZ4_CHECK(source != NULL);
    fill_pcm_like_samples((int16_t *)source, SAMPLE_COUNT);

    const int compressed_capacity = LZ4_compressBound((int)source_size);
    LZ4_CHECK(compressed_capacity > 0);

    char *compressed = malloc((size_t)compressed_capacity);
    uint8_t *restored = malloc(source_size);
    LZ4_CHECK((compressed != NULL) && (restored != NULL));

    const int compressed_size = LZ4_compress_default(
                                    (const char *)source, compressed, (int)source_size, compressed_capacity);
    LZ4_CHECK(compressed_size > 0);

    const int restored_size = LZ4_decompress_safe(
                                  compressed, (char *)restored, compressed_size, (int)source_size);
    LZ4_CHECK((restored_size == (int)source_size) &&
              (memcmp(source, restored, source_size) == 0));

    printf("source_size=%u compressed_size=%d verify=true\n",
           (unsigned)source_size, compressed_size);

    free(restored);
    free(compressed);
    free(source);
}
