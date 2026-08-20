/*
 * SPDX-FileCopyrightText: 2017-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_attr.h"
#include "esp_trace_port_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

extern esp_trace_encoder_t *s_sysview_encoder;
extern esp_trace_link_types_t s_sysview_link_type;

FORCE_INLINE_ATTR esp_trace_encoder_t *SEGGER_SYSVIEW_ESP_GetEncoder(void)
{
    return s_sysview_encoder;
}

FORCE_INLINE_ATTR esp_trace_link_types_t SEGGER_SYSVIEW_ESP_GetLinkType(void)
{
    return s_sysview_link_type;
}

#ifdef __cplusplus
}
#endif
