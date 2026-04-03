#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_twai_onchip.h"
#include "esp_twai.h"
#include "CANopen.h"
#include "OD.h"

static CO_t *CO = NULL;

/* esp_canopennode is built with CO_MULTIPLE_OD, so CO_new() needs this config. */
static CO_config_t co_config = {
    .CNT_NMT = 1,
    .ENTRY_H1017 = &OD_ENTRY_H1017,
    .CNT_EM = 1,
    .ENTRY_H1001 = &OD_ENTRY_H1001,
    .ENTRY_H1014 = &OD_ENTRY_H1014,
    .CNT_ARR_1003 = 8,
    .ENTRY_H1003 = &OD_ENTRY_H1003,
};

void app_main(void)
{
    twai_node_handle_t node_hdl;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 4,
        .io_cfg.rx = 4,
        .io_cfg.quanta_clk_out = GPIO_NUM_NC,
        .io_cfg.bus_off_indicator = GPIO_NUM_NC,
        .bit_timing.bitrate = 200000,
        .tx_queue_depth = 5,
        .flags.enable_self_test = true,
        .flags.enable_loopback = true,
    };
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    CO = CO_new(&co_config, NULL);
    CO_CANinit(CO, node_hdl, 500);
    CO_CANopenInit(
        CO,
        NULL,
        NULL,
        OD,
        NULL,
        0,
        0,
        0,
        0,
        false,
        1,          // Node ID
        NULL
    );

    CO_CANsetNormalMode(CO->CANmodule);
    CO_NMT_sendInternalCommand(CO->NMT, CO_NMT_ENTER_OPERATIONAL);

    int64_t last_us = esp_timer_get_time();
    while (1) {
        int64_t now_us = esp_timer_get_time();
        CO_process(CO, false, (uint32_t)(now_us - last_us), NULL);
        last_us = now_us;

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    CO_delete(CO);
    twai_node_delete(node_hdl);
}
