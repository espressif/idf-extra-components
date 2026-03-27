/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "301/CO_driver.h"
#include "esp_log.h"
#include "esp_twai.h"
#include <stdio.h>

static const char *TAG = "CO_driver_esp32";

#define CO_ID_FLAG_RTR   0x8000U /*!< RTR flag, part of identifier */

static bool co_twai_rx_done_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    CO_CANmodule_t *CANmodule = (CO_CANmodule_t *)user_ctx;
    if (!CANmodule || !CANmodule->CANptr) {
        return false;
    }

    CO_CANrxMsg_t msg = {0};
    twai_frame_t rx_frame = {
        .buffer = msg.data,
        .buffer_len = sizeof(msg.data),
    };
    if (ESP_OK != twai_node_receive_from_isr((twai_node_handle_t)CANmodule->CANptr, &rx_frame)) {
        return false;
    }
    msg.ident = (uint16_t)(rx_frame.header.id & 0x07FFU);
    msg.DLC = (uint8_t)rx_frame.header.dlc;
    if (rx_frame.header.rtr) {
        msg.ident |= CO_ID_FLAG_RTR;
    }

    for (uint16_t i = 0U; i < CANmodule->rxSize; i++) {
        CO_CANrx_t *buffer = &CANmodule->rxArray[i];
        if (buffer->CANrx_callback && (((msg.ident ^ buffer->ident) & buffer->mask) == 0U)) {
            buffer->CANrx_callback(buffer->object, &msg);
            break;
        }
    }
    return true;
}

void CO_CANsetConfigurationMode(void *CANptr)
{
    (void)CANptr;   // not used
}

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, void *CANptr, CO_CANrx_t rxArray[], uint16_t rxSize,
                                   CO_CANtx_t txArray[], uint16_t txSize, uint16_t CANbitRate)
{
    /* TWAI bitrate/configuration is owned by the application layer. */
    (void)CANbitRate;

    if (!CANmodule || !CANptr || !rxArray || !txArray) {
        ESP_LOGW(TAG, "CO_CANmodule_init invalid args: CANmodule=%p CANptr=%p rxArray=%p txArray=%p", (void *)CANmodule,
                 CANptr, (void *)rxArray, (void *)txArray);
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0U;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    uint16_t i;
    for (i = 0U; i < rxSize; i++) {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (i = 0U; i < txSize; i++) {
        txArray[i].bufferFull = false;
        txArray[i].syncFlag = false;
    }

    twai_event_callbacks_t cbs = {
        .on_rx_done = co_twai_rx_done_callback,
    };
    if (ESP_OK != twai_node_register_event_callbacks((twai_node_handle_t)CANptr, &cbs, CANmodule)) {
        ESP_LOGW(TAG, "CO_CANmodule_init twai_node_register_event_callbacks failed");
        return CO_ERROR_SYSCALL;
    }
    twai_node_enable((twai_node_handle_t)CANptr);
    CANmodule->CANnormal = true;

    return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    if (!CANmodule || !CANmodule->CANptr) {
        ESP_LOGW(TAG, "CO_CANmodule_disable invalid args: CANmodule=%p CANptr=%p", (void *)CANmodule,
                 CANmodule ? CANmodule->CANptr : NULL);
        return;
    }

    if (ESP_OK != twai_node_disable((twai_node_handle_t)CANmodule->CANptr)) {
        ESP_LOGW(TAG, "CO_CANmodule_disable twai_node_disable failed");
    }
    CANmodule->CANnormal = false;
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr,
                                    void *object, void (*CANrx_callback)(void *object, void *message))
{
    CO_CANrx_t *buffer;

    if (!CANmodule || !object || !CANrx_callback || (index >= CANmodule->rxSize)) {
        ESP_LOGW(TAG, "CO_CANrxBufferInit invalid args: CANmodule=%p index=%u object=%p cb=%p", (void *)CANmodule,
                 index, object, (void *)CANrx_callback);
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    buffer = &CANmodule->rxArray[index];
    buffer->object = object;
    buffer->CANrx_callback = CANrx_callback;
    buffer->ident = ident & 0x07FFU;
    if (rtr) {
        buffer->ident |= CO_ID_FLAG_RTR;
    }
    buffer->mask = (mask & 0x07FFU) | CO_ID_FLAG_RTR;

    return CO_ERROR_NO;
}

CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                               bool_t syncFlag)
{
    CO_CANtx_t *buffer;

    if (!CANmodule || (index >= CANmodule->txSize) || (noOfBytes > 8U)) {
        ESP_LOGW(TAG, "CO_CANtxBufferInit invalid args: CANmodule=%p index=%u noOfBytes=%u", (void *)CANmodule, index,
                 noOfBytes);
        return NULL;
    }

    buffer = &CANmodule->txArray[index];
    buffer->ident = ((uint32_t)ident & 0x07FFU) | ((uint32_t)(((uint32_t)noOfBytes & 0xFU) << 11U))
                    | ((uint32_t)(rtr ? 0x8000U : 0U));
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    const uint32_t tx_timeout_ticks = 1U;
    twai_frame_t tx_frame = {0};

    if (buffer->bufferFull) {
        if (!CANmodule->firstCANtxMessage) {
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }
        return CO_ERROR_TX_OVERFLOW;
    }

    tx_frame.header.id = buffer->ident & 0x07FFU;
    tx_frame.header.ide = false;
    tx_frame.buffer = buffer->data;
    tx_frame.buffer_len = buffer->DLC;

    if (ESP_OK != twai_node_transmit((twai_node_handle_t)CANmodule->CANptr, &tx_frame, tx_timeout_ticks)) {
        ESP_LOGW(TAG, "CO_CANsend twai_node_transmit failed");
        return CO_ERROR_TX_BUSY;
    }

    CANmodule->bufferInhibitFlag = buffer->syncFlag;
    CANmodule->firstCANtxMessage = false;
    return CO_ERROR_NO;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    if (!CANmodule) {
        return;
    }

    bool_t tpdoDeleted = false;
    for (uint16_t i = 0U; i < CANmodule->txSize; i++) {
        CO_CANtx_t *buffer = &CANmodule->txArray[i];
        if (buffer->bufferFull && buffer->syncFlag) {
            buffer->bufferFull = false;
            tpdoDeleted = true;
            if (CANmodule->CANtxCount > 0U) {
                CANmodule->CANtxCount--;
            }
        }
    }

    CANmodule->bufferInhibitFlag = false;
    if (tpdoDeleted) {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}

void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    /* Keep snapshot for change detection; detailed TWAI error-counter mapping is added later. */
    CANmodule->errOld = CANmodule->CANerrorStatus;
}
