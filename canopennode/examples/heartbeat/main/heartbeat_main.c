/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_twai_onchip.h"
#include "esp_twai.h"
#include "CANopen.h"
#include "OD.h"


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

    CO_NMT_reset_cmd_t reset = CO_RESET_COMM;
    while (reset != CO_RESET_APP) {
        /* Standard CANopenNode communication reset sequence */
        CO_CANmodule_disable(CO->CANmodule);
        CO_CANinit(CO, node_hdl, node_config.bit_timing.bitrate / 1000);
        CO_CANopenInit(CO, NULL, NULL, OD, NULL, 0, 0, 0, 0, false, 1, NULL);
        CO_CANsetNormalMode(CO->CANmodule);

        reset = CO_RESET_NOT;
        int64_t last_us = esp_timer_get_time();
        while (reset == CO_RESET_NOT) {
            int64_t now_us = esp_timer_get_time();
            reset = CO_process(CO, false, (uint32_t)(now_us - last_us), NULL);
            last_us = now_us;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    CO_delete(CO);
    twai_node_delete(node_hdl);
}
