/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"

#include "esp_trace_port_encoder.h"
#include "esp_trace_port_transport.h"
#include "SEGGER_SYSVIEW_esp.h"

static const char *TAG = "sysview-esp";

esp_trace_encoder_t *s_sysview_encoder;
esp_trace_link_types_t s_sysview_link_type;

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

    s_sysview_link_type = enc->tp->vt->get_link_type(enc->tp);
    s_sysview_encoder = enc;

    return 0;
}
