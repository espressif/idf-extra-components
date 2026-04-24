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
    CANOPEN_NODE_ID = 1,
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

#ifdef CONFIG_CO_SDO_CLIENT
static bool sdo_client_upload_visible_string(CO_SDOclient_t *sdo_client, uint16_t index, uint8_t sub_index,
        uint8_t *buf, size_t buf_size)
{
    CO_SDO_abortCode_t abort_code = CO_SDO_AB_NONE;
    CO_SDO_return_t ret;
    uint32_t elapsed_us = 0;
    size_t transferred = 0;

    if (buf_size == 0) {
        return false;
    }

    ret = CO_SDOclientUploadInitiate(sdo_client, index, sub_index, SDO_CLIENT_TIMEOUT_MS, false);
    if (ret != CO_SDO_RT_ok_communicationEnd) {
        ESP_LOGE(TAG, "SDO upload initiate failed for 0x%04X:%u, ret=%d", index, sub_index, ret);
        CO_SDOclientClose(sdo_client);
        return false;
    }

    do {
        ret = CO_SDOclientUpload(sdo_client, 10000, false, &abort_code, NULL, &transferred, NULL);
        elapsed_us += 10000;
        if (ret < 0) {
            ESP_LOGE(TAG, "SDO upload failed for 0x%04X:%u, abort=0x%08" PRIX32, index, sub_index, abort_code);
            CO_SDOclientClose(sdo_client);
            return false;
        }
    } while ((ret > 0) && (elapsed_us < (SDO_CLIENT_TIMEOUT_MS * 1000U)));

    transferred = CO_SDOclientUploadBufRead(sdo_client, buf, buf_size - 1U);
    buf[transferred] = '\0';
    CO_SDOclientClose(sdo_client);

    if (ret > 0) {
        ESP_LOGE(TAG, "SDO upload timed out for 0x%04X:%u", index, sub_index);
        return false;
    }

    return true;
}

static bool sdo_client_download_bytes(CO_SDOclient_t *sdo_client, uint16_t index, uint8_t sub_index,
                                      const uint8_t *data, size_t data_len)
{
    CO_SDO_abortCode_t abort_code = CO_SDO_AB_NONE;
    CO_SDO_return_t ret;
    uint32_t elapsed_us = 0;
    size_t written;

    ret = CO_SDOclientDownloadInitiate(sdo_client, index, sub_index, data_len, SDO_CLIENT_TIMEOUT_MS, false);
    if (ret != CO_SDO_RT_ok_communicationEnd) {
        ESP_LOGE(TAG, "SDO download initiate failed for 0x%04X:%u, ret=%d", index, sub_index, ret);
        CO_SDOclientClose(sdo_client);
        return false;
    }

    written = CO_SDOclientDownloadBufWrite(sdo_client, data, data_len);
    if (written != data_len) {
        ESP_LOGE(TAG, "SDO download buffer short write for 0x%04X:%u, wrote=%u expected=%u",
                 index, sub_index, (unsigned)written, (unsigned)data_len);
        CO_SDOclientClose(sdo_client);
        return false;
    }

    do {
        ret = CO_SDOclientDownload(sdo_client, 10000, false, false, &abort_code, NULL, NULL);
        elapsed_us += 10000;
        if (ret < 0) {
            ESP_LOGE(TAG, "SDO download failed for 0x%04X:%u, abort=0x%08" PRIX32, index, sub_index, abort_code);
            CO_SDOclientClose(sdo_client);
            return false;
        }
    } while ((ret > 0) && (elapsed_us < (SDO_CLIENT_TIMEOUT_MS * 1000U)));

    CO_SDOclientClose(sdo_client);

    if (ret > 0) {
        ESP_LOGE(TAG, "SDO download timed out for 0x%04X:%u", index, sub_index);
        return false;
    }

    return true;
}

static void run_sdo_client_self_test(CO_t *co)
{
    static const uint8_t new_name[] = "cli-self";
    uint8_t read_buf[32];
    CO_SDOclient_t *sdo_client;
    CO_SDO_return_t ret;

    if ((co == NULL) || (co->SDOclient == NULL)) {
        ESP_LOGW(TAG, "SDO client self-test skipped: no client instance");
        return;
    }

    sdo_client = &co->SDOclient[0];
    ret = CO_SDOclient_setup(sdo_client, CO_CAN_ID_SDO_CLI + CANOPEN_NODE_ID, CO_CAN_ID_SDO_SRV + CANOPEN_NODE_ID,
                             CANOPEN_NODE_ID);
    if (ret != CO_SDO_RT_ok_communicationEnd) {
        ESP_LOGE(TAG, "SDO client setup failed, ret=%d", ret);
        return;
    }

    if (!sdo_client_upload_visible_string(sdo_client, 0x1008, 0, read_buf, sizeof(read_buf))) {
        return;
    }
    ESP_LOGI(TAG, "SDO client read 0x1008 -> '%s'", (char *)read_buf);

    if (!sdo_client_download_bytes(sdo_client, 0x1008, 0, new_name, sizeof(new_name) - 1U)) {
        return;
    }
    ESP_LOGI(TAG, "SDO client wrote 0x1008 <- '%s'", (const char *)new_name);

    if (!sdo_client_upload_visible_string(sdo_client, 0x1008, 0, read_buf, sizeof(read_buf))) {
        return;
    }
    ESP_LOGI(TAG, "SDO client verify 0x1008 -> '%s'", (char *)read_buf);
}
#endif

void app_main(void)
{
    twai_node_handle_t node_hdl;
    bool sdo_client_self_test_done = false;
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
        uint32_t err_info = 0;
        /* Standard CANopenNode communication reset sequence */
        CO_CANmodule_disable(CO->CANmodule);
        CO_CANinit(CO, node_hdl, node_config.bit_timing.bitrate / 1000);
        CO_CANopenInit(CO, NULL, NULL, OD, NULL, 0, FIRST_HEARTBEAT_TIME_MS, SDO_SERVER_TIMEOUT_MS,
                       SDO_CLIENT_TIMEOUT_MS, false, CANOPEN_NODE_ID, &err_info);
#ifdef CONFIG_CO_PDO
        CO_CANopenInitPDO(CO, CO->em, OD, CANOPEN_NODE_ID, &err_info);
#endif
        CO_CANsetNormalMode(CO->CANmodule);

#ifdef CONFIG_CO_SDO_CLIENT
        // Testing SDO client interact with local loop
        if (!sdo_client_self_test_done) {
            run_sdo_client_self_test(CO);
            sdo_client_self_test_done = true;
        }
#endif

        reset = CO_RESET_NOT;
        int64_t last_us = esp_timer_get_time();
        while (reset == CO_RESET_NOT) {
            int64_t now_us = esp_timer_get_time();
            uint32_t time_diff_us = (uint32_t)(now_us - last_us);
            reset = CO_process(CO, false, time_diff_us, NULL);
#ifdef CONFIG_CO_PDO
            CO_process_RPDO(CO, false, time_diff_us, NULL);
            CO_process_TPDO(CO, false, time_diff_us, NULL);
#endif
            last_us = now_us;
            vTaskDelay(pdMS_TO_TICKS(CANOPEN_TASK_PERIOD_MS));
        }
    }

    CO_delete(CO);
    twai_node_delete(node_hdl);
}
