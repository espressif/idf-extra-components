/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SystemView backend for esp_trace compiler-instrumented function tracing.
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_cpu.h"
#include "esp_trace_port_encoder.h"
#include "adapter_encoder_sysview.h"
#include "SEGGER_SYSVIEW.h"

/* Event IDs sent as EventOffset + id. EventOffset is allocated in registration order starting at 512. */
enum {
    FT_EVT_ENTER = 0,
    FT_EVT_EXIT,
    FT_EVT_COUNT,
};

static SEGGER_SYSVIEW_MODULE s_ft_module = {
    .sModule =
    "M=ESP_FunctionTrace, "
    "0 function_enter func=%p call_site=%p, "
    "1 function_exit func=%p call_site=%p",
    .NumEvents = FT_EVT_COUNT,
};

static bool s_registered;

static inline bool should_drop_for_cpu(struct esp_trace_encoder *enc)
{
    sysview_encoder_ctx_t *ctx = enc->ctx;
    return ctx && ctx->filter_by_cpu && (esp_cpu_get_core_id() != ctx->dest_cpu);
}

void esp_sysview_function_trace_register(void)
{
    if (!s_registered) {
        SEGGER_SYSVIEW_RegisterModule(&s_ft_module);
        s_registered = true;
    }
}

void esp_sysview_function_enter(struct esp_trace_encoder *enc, void *func, void *call_site)
{
    if (!s_registered || should_drop_for_cpu(enc)) {
        return;
    }
    U8 aPacket[SEGGER_SYSVIEW_INFO_SIZE + 2 * SEGGER_SYSVIEW_QUANTA_U32];
    U8 *pPayload = SEGGER_SYSVIEW_PREPARE_PACKET(aPacket);
    pPayload = SEGGER_SYSVIEW_EncodeU32(pPayload, (U32)(uintptr_t)func);
    pPayload = SEGGER_SYSVIEW_EncodeU32(pPayload, (U32)(uintptr_t)call_site);
    SEGGER_SYSVIEW_SendPacket(aPacket, pPayload, s_ft_module.EventOffset + FT_EVT_ENTER);
}

void esp_sysview_function_exit(struct esp_trace_encoder *enc, void *func, void *call_site)
{
    if (!s_registered || should_drop_for_cpu(enc)) {
        return;
    }
    U8 aPacket[SEGGER_SYSVIEW_INFO_SIZE + 2 * SEGGER_SYSVIEW_QUANTA_U32];
    U8 *pPayload = SEGGER_SYSVIEW_PREPARE_PACKET(aPacket);
    pPayload = SEGGER_SYSVIEW_EncodeU32(pPayload, (U32)(uintptr_t)func);
    pPayload = SEGGER_SYSVIEW_EncodeU32(pPayload, (U32)(uintptr_t)call_site);
    SEGGER_SYSVIEW_SendPacket(aPacket, pPayload, s_ft_module.EventOffset + FT_EVT_EXIT);
}
