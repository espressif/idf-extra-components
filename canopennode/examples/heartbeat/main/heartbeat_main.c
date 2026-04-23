/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai_onchip.h"
#include "esp_twai.h"
#include "CANopen.h"
#include "OD.h"

static const char *TAG = "heartbeat";

enum {
    FIRST_HEARTBEAT_TIME_MS = 1000,
    SDO_SERVER_TIMEOUT_MS = 1000,
    SDO_CLIENT_TIMEOUT_MS = 500,
    CANOPEN_TASK_PERIOD_MS = 10,
};

static OD_extension_t od_1008_extension;

static ODR_t od_write_1008(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
    ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);
    if (ret == ODR_OK) {
        ESP_EARLY_LOGI(TAG, "0x1008 updated to '%s'", OD_RAM.x1008_manufacturerDeviceName);
    }
    return ret;
}

void app_main(void)
{
    twai_node_handle_t node_hdl;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 4,
        .io_cfg.rx = 5,
        .io_cfg.quanta_clk_out = GPIO_NUM_NC,
        .io_cfg.bus_off_indicator = GPIO_NUM_NC,
        .bit_timing.bitrate = 200000,
        .tx_queue_depth = 5,
    };
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    CO_t *CO = CO_new(NULL, NULL);
    od_1008_extension.object = NULL;
    od_1008_extension.read = OD_readOriginal;
    od_1008_extension.write = od_write_1008;
    (void)OD_extension_init(OD_ENTRY_H1008_manufacturerDeviceName, &od_1008_extension);

    CO_NMT_reset_cmd_t reset = CO_RESET_COMM;
    while (reset != CO_RESET_APP) {
        /* Standard CANopenNode communication reset sequence */
        CO_CANmodule_disable(CO->CANmodule);
        CO_CANinit(CO, node_hdl, node_config.bit_timing.bitrate / 1000);
        CO_CANopenInit(CO, NULL, NULL, OD, NULL, 0, FIRST_HEARTBEAT_TIME_MS, SDO_SERVER_TIMEOUT_MS,
                       SDO_CLIENT_TIMEOUT_MS, false, 1, NULL);
        CO_CANsetNormalMode(CO->CANmodule);

        reset = CO_RESET_NOT;
        int64_t last_us = esp_timer_get_time();
        while (reset == CO_RESET_NOT) {
            int64_t now_us = esp_timer_get_time();
            reset = CO_process(CO, false, (uint32_t)(now_us - last_us), NULL);
            last_us = now_us;
            vTaskDelay(pdMS_TO_TICKS(CANOPEN_TASK_PERIOD_MS));
        }
    }

    CO_delete(CO);
    twai_node_delete(node_hdl);
}
