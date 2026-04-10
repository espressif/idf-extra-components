/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This is an integration benchmark, not a replacement for the upstream LZ4
 * correctness or performance suite. It answers a narrower question: can the
 * LZ4 component run reliably on an ESP-IDF target, and what are the basic
 * Block API metrics on that target?
 *
 * The input is a deterministic PCM-like signal. It is intentionally smooth
 * with a small amount of noise, so it represents a compressible raw sensor or
 * audio buffer while remaining reproducible on every board. Results should be
 * compared only between the same target, CPU frequency, optimization level,
 * and memory configuration. They are not universal LZ4 performance claims.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "lz4.h"
#include "unity.h"

enum {
    /* 16-bit samples: 16 KiB of input data. */
    PCM_SAMPLE_COUNT = 8192,
    /* Enough repetitions to reduce timer granularity noise without making the
     * integration test unnecessarily long. */
    BENCH_ITERATIONS = 300,
};

typedef struct {
    /* Compressed bytes divided by original bytes; lower is better. */
    double ratio;
    /* Throughput for the Block compressor/decompressor; higher is better. */
    double block_comp_mbps;
    double block_decomp_mbps;
    /* One compression + decompression round trip; lower is better. */
    int64_t latency_us;
    /* Functional gate: 1 means the decompressed data matches the input. */
    int verify;
} bench_results_t;

static void print_bench_results(const bench_results_t *results)
{
    /* Keep these keys stable: CI and local scripts use them to compare runs. */
    printf("BENCH_RATIO=%.6f\n", results->ratio);
    printf("BENCH_BLOCK_COMP_MBPS=%.3f\n", results->block_comp_mbps);
    printf("BENCH_BLOCK_DECOMP_MBPS=%.3f\n", results->block_decomp_mbps);
    printf("BENCH_LATENCY_US=%lld\n", (long long)results->latency_us);
    printf("BENCH_VERIFY=%d\n", results->verify);
}

static void fill_pcm_like_samples(int16_t *samples, size_t sample_count)
{
    uint32_t state = 0x5A17B3C1u;
    int32_t current = 0;

    for (size_t i = 0; i < sample_count; i++) {
        state = (state * 1664525u) + 1013904223u;

        const int32_t saw = (int32_t)(i % 192u) - 96;
        const int32_t noise = (int32_t)((state >> 27) & 0x1Fu) - 16;
        const int32_t target = saw * 192;

        current += (target - current) / 10;
        current += noise;

        if (current > INT16_MAX) {
            current = INT16_MAX;
        } else if (current < INT16_MIN) {
            current = INT16_MIN;
        }

        samples[i] = (int16_t)current;
    }
}

static double elapsed_us_to_mbps(int64_t elapsed_us, size_t total_bytes)
{
    if (elapsed_us <= 0) {
        return 0.0;
    }

    const double seconds = (double)elapsed_us / 1000000.0;
    return ((double)total_bytes / seconds) / 1000000.0;
}

TEST_CASE("lz4 block benchmark reports verified metrics", "[lz4][benchmark]")
{
    bench_results_t results = {
        .ratio = -1.0,
        .block_comp_mbps = -1.0,
        .block_decomp_mbps = -1.0,
        .latency_us = -1,
        .verify = 0,
    };

    const size_t pcm_size = PCM_SAMPLE_COUNT * sizeof(int16_t);
    printf("LZ4 Block benchmark: %u bytes, %d iterations\n",
           (unsigned)pcm_size, BENCH_ITERATIONS);
    printf("Compare like-for-like builds: lower ratio/latency and higher throughput are better;\n");

    int16_t *pcm = malloc(pcm_size);
    if (pcm == NULL) {
        TEST_FAIL_MESSAGE("PCM allocation failed");
    }

    fill_pcm_like_samples(pcm, PCM_SAMPLE_COUNT);

    const int max_compressed_size = LZ4_compressBound((int)pcm_size);
    if (max_compressed_size <= 0) {
        free(pcm);
        TEST_FAIL_MESSAGE("LZ4_compressBound failed");
    }

    char *compressed = malloc((size_t)max_compressed_size);
    int16_t *restored = malloc(pcm_size);
    if ((compressed == NULL) || (restored == NULL)) {
        free(restored);
        free(compressed);
        free(pcm);
        TEST_FAIL_MESSAGE("Benchmark buffer allocation failed");
    }

    const int compressed_size = LZ4_compress_default((const char *)pcm,
                                                     compressed,
                                                     (int)pcm_size,
                                                     max_compressed_size);
    const int decoded_size = (compressed_size > 0)
                             ? LZ4_decompress_safe(compressed,
                                                   (char *)restored,
                                                   compressed_size,
                                                   (int)pcm_size)
                             : -1;

    const bool verified = (compressed_size > 0)
                          && (decoded_size == (int)pcm_size)
                          && (memcmp(pcm, restored, pcm_size) == 0);

    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        (void)LZ4_compress_default((const char *)pcm,
                                   compressed,
                                   (int)pcm_size,
                                   max_compressed_size);
    }
    const int64_t comp_elapsed_us = esp_timer_get_time() - t0;

    int64_t decomp_elapsed_us = 0;
    if (compressed_size > 0) {
        t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            (void)LZ4_decompress_safe(compressed,
                                      (char *)restored,
                                      compressed_size,
                                      (int)pcm_size);
        }
        decomp_elapsed_us = esp_timer_get_time() - t0;
    }

    // Measure one complete Block round trip separately from the throughput loops.
    t0 = esp_timer_get_time();
    const int latency_comp_size = LZ4_compress_default((const char *)pcm,
                                                       compressed,
                                                       (int)pcm_size,
                                                       max_compressed_size);
    const int latency_decoded_size = (latency_comp_size > 0)
                                     ? LZ4_decompress_safe(compressed,
                                                           (char *)restored,
                                                           latency_comp_size,
                                                           (int)pcm_size)
                                     : -1;
    const int64_t latency_us = esp_timer_get_time() - t0;

    const bool latency_verified = (latency_comp_size > 0)
                                  && (latency_decoded_size == (int)pcm_size)
                                  && (memcmp(pcm, restored, pcm_size) == 0);

    const bool final_verify = verified && latency_verified;

    results.ratio = (compressed_size > 0)
                    ? ((double)compressed_size / (double)pcm_size)
                    : -1.0;
    results.block_comp_mbps = elapsed_us_to_mbps(comp_elapsed_us,
                                                 pcm_size * BENCH_ITERATIONS);
    results.block_decomp_mbps = (compressed_size > 0)
                                ? elapsed_us_to_mbps(decomp_elapsed_us,
                                                     pcm_size * BENCH_ITERATIONS)
                                : -1.0;
    results.latency_us = latency_us;
    results.verify = final_verify ? 1 : 0;

    print_bench_results(&results);

    free(restored);
    free(compressed);
    free(pcm);

    TEST_ASSERT_EQUAL_INT(1, results.verify);
}
