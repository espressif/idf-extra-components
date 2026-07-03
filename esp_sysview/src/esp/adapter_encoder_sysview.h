/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include "esp_trace_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SystemView encoder configuration
 */
typedef struct {
    /**
     * @brief CPU to trace (0 or 1)
     *
     * UART apptrace carries one SystemView stream, so events are filtered to
     * this CPU. JTAG tracing keeps multicore events and ignores this value.
     *
     * - 0: Capture events from CPU0 (Pro CPU)
     * - 1: Capture events from CPU1 (App CPU)
     *
     * @note This parameter is ignored for single-core systems
     * @note This parameter is ignored when using JTAG transport
     */
    int dest_cpu;
} esp_trace_sysview_config_t;

/**
 * @brief SystemView encoder context structure
 *
 * This structure is shared between the sysview adapter and the SEGGER RTT layer
 * to allow proper access to encoder-specific configuration.
 */
typedef struct {
    int dest_cpu;                        ///< CPU to trace (0 or 1)
    bool filter_by_cpu;                  ///< true for single-stream transports that cannot represent multicore events
    esp_trace_lock_t lock;               ///< Encoder lock
} sysview_encoder_ctx_t;

/* Forward declaration to avoid pulling the encoder port header here. */
struct esp_trace_encoder;

/** @brief Send a function entry event (esp_trace function-trace callback). */
void esp_sysview_function_enter(struct esp_trace_encoder *enc, void *func, void *call_site);

/** @brief Send a function exit event (esp_trace function-trace callback). */
void esp_sysview_function_exit(struct esp_trace_encoder *enc, void *func, void *call_site);

/** @brief Register the function-trace SystemView module (call once at init). */
void esp_sysview_function_trace_register(void);

#ifdef __cplusplus
}
#endif
