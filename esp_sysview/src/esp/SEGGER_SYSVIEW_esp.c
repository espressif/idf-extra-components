/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"

#include "esp_trace_port_encoder.h"
#include "esp_trace_port_transport.h"
#include "adapter_encoder_sysview.h"

static const char *TAG = "sysview-esp";

/**
 * @brief Encoder reference used by RTT layer
 * Set by SEGGER_SYSVIEW_ESP_SetEncoder() during encoder init.
 */
static esp_trace_encoder_t *s_encoder = NULL;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       SEGGER_SYSVIEW_ESP_SetEncoder()
*
*  Function description
*    Inject encoder handle from esp_trace adapter.
*    This allows SEGGER RTT to access transport through the encoder's
*    transport reference.
*
*  Parameters
*    encoder  Pointer to encoder instance from esp_trace
*
*  Return value
*    0 if successful, -1 if encoder is not initialized or missing required functions in transport.
*/
int SEGGER_SYSVIEW_ESP_SetEncoder(void *encoder)
{
    esp_trace_encoder_t *enc = encoder;
    /* Check if adapter has all required functions */
    if (!enc || !enc->ctx ||
            !enc->vt->give_lock ||
            !enc->vt->take_lock ||
            !enc->tp->vt->down_buffer_config ||
            !enc->tp->vt->write ||
            !enc->tp->vt->flush_nolock ||
            !enc->tp->vt->read ||
            !enc->tp->vt->get_link_type) {
        ESP_LOGE(TAG, "Encoder not initialized or missing required functions in transport");
        return -1;
    }

    s_encoder = enc;

    return 0;
}

/*********************************************************************
*
*       SEGGER_SYSVIEW_ESP_GetEncoder()
*
*  Function description
*    Returns the encoder handle for accessing transport functions.
*    This is used by SEGGER_SYSVIEW_Config_FreeRTOS.c to access
*    transport lock functions.
*
*  Parameters
*    None
*
*  Return value
*    Pointer to encoder instance, or NULL if not initialized.
*/
void *SEGGER_SYSVIEW_ESP_GetEncoder(void)
{
    return s_encoder;
}

/*********************************************************************
*
*       SEGGER_SYSVIEW_ESP_GetDestCpu()
*
*  Function description
*    Gets the destination CPU from the encoder context.
*
*  Parameters
*    None
*
*  Return value
*    CPU ID (0 or 1) to trace
*/
int SEGGER_SYSVIEW_ESP_GetDestCpu(void)
{
    sysview_encoder_ctx_t *ctx = s_encoder->ctx;
    return ctx->dest_cpu;
}
