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
#include "lz4frame.h"

#include "lz4_example.h"

#define LZ4_CHECK(condition) ESP_ERROR_CHECK((condition) ? ESP_OK : ESP_FAIL)
#define LZ4F_CHECK(code) ESP_ERROR_CHECK(LZ4F_isError(code) ? ESP_FAIL : ESP_OK)

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

void lz4_run_stream_example(void)
{
    enum {
        SAMPLE_COUNT = 4096,
        INPUT_CHUNK = 7,
        OUTPUT_CHUNK = 31,
        MAX_DECOMPRESS_STEPS = 4096,
    };
    const size_t source_size = SAMPLE_COUNT * sizeof(int16_t);

    printf("\nFrame streaming API example\n");

    uint8_t *source = malloc(source_size);
    LZ4_CHECK(source != NULL);
    fill_pcm_like_samples((int16_t *)source, SAMPLE_COUNT);

    LZ4F_preferences_t preferences = { 0 };
    preferences.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    const size_t frame_capacity = LZ4F_compressFrameBound(source_size, &preferences);
    LZ4F_CHECK(frame_capacity);

    uint8_t *frame = malloc(frame_capacity);
    uint8_t *restored = malloc(source_size);
    LZ4_CHECK((frame != NULL) && (restored != NULL));

    const size_t frame_size = LZ4F_compressFrame(
                                  frame, frame_capacity, source, source_size, &preferences);
    LZ4F_CHECK(frame_size);

    LZ4F_dctx *decompression_context = NULL;
    LZ4F_CHECK(LZ4F_createDecompressionContext(
                   &decompression_context, LZ4F_VERSION));

    size_t source_offset = 0;
    size_t restored_offset = 0;
    size_t result = 1;
    for (size_t step = 0;
            step < MAX_DECOMPRESS_STEPS && result != 0;
            step++) {
        size_t input_size = (frame_size - source_offset < INPUT_CHUNK)
                            ? frame_size - source_offset : INPUT_CHUNK;
        size_t output_size = (source_size - restored_offset < OUTPUT_CHUNK)
                             ? source_size - restored_offset : OUTPUT_CHUNK;

        result = LZ4F_decompress(decompression_context,
                                 restored + restored_offset, &output_size,
                                 frame + source_offset, &input_size, NULL);
        LZ4F_CHECK(result);
        source_offset += input_size;
        restored_offset += output_size;
        LZ4_CHECK((input_size > 0) || (output_size > 0) || (result == 0));
    }

    LZ4_CHECK((result == 0) && (source_offset == frame_size) &&
              (restored_offset == source_size) &&
              (memcmp(source, restored, source_size) == 0));
    LZ4F_CHECK(LZ4F_freeDecompressionContext(decompression_context));

    printf("source_size=%u frame_size=%u verify=true\n",
           (unsigned)source_size, (unsigned)frame_size);

    free(restored);
    free(frame);
    free(source);
}
