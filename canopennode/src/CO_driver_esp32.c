/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "301/CO_driver.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_twai.h"
#include <stdio.h>

static const char *TAG = "CO_driver_esp32";

#if CO_CONFIG_SDO_SRV == 0
// Provide empty implementations for SDO server functions to avoid build errors when SDO server is disabled
void CO_SDOserver_init(void *SDO, void *CANdev, void *OD) {}
void CO_SDOserver_process(void *SDO, bool_t NMTisPreOrOperational, uint32_t timeDifference_us, uint32_t *timerNext_us) {}
#endif

static bool co_twai_rx_done_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    CO_CANmodule_t *CANmodule = (CO_CANmodule_t *)user_ctx;

    uint8_t rx_buffer[8];
    twai_frame_t rx_frame = {
        .buffer = rx_buffer,
        .buffer_len = sizeof(rx_buffer),
    };
    ESP_RETURN_ON_FALSE_ISR(ESP_OK == twai_node_receive_from_isr((twai_node_handle_t)CANmodule->CANptr, &rx_frame), false, TAG, "co_twai_rx_done_callback twai_node_receive_from_isr failed");

    ESP_EARLY_LOGD(TAG, "Rx %3lx [%d] %x %x %x %x %x %x %x %x", rx_frame.header.id, rx_frame.header.dlc, rx_frame.buffer[0], rx_frame.buffer[1], rx_frame.buffer[2], rx_frame.buffer[3], rx_frame.buffer[4], rx_frame.buffer[5], rx_frame.buffer[6], rx_frame.buffer[7]);
    rx_frame.header.id |= rx_frame.header.rtr ? CO_ID_FLAG_RTR : 0U;
    for (uint16_t i = 0U; i < CANmodule->rxSize; i++) {
        CO_CANrx_t *buffer = &CANmodule->rxArray[i];
        if ((((rx_frame.header.id ^ buffer->ident) & buffer->mask) == 0U) && buffer->CANrx_callback) {
            buffer->CANrx_callback(buffer->object, &rx_frame);
            break;
        }
    }
    return true;
}

static bool co_twai_state_callback(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx)
{
    CO_CANmodule_t *CANmodule = (CO_CANmodule_t *)user_ctx;
    const char *twai_state_name[] = {"ERROR_ACTIVE", "ERROR_WARNING", "ERROR_PASSIVE", "BUS_OFF"};
    ESP_EARLY_LOGI(TAG, "State changed: %s -> %s", twai_state_name[edata->old_sta], twai_state_name[edata->new_sta]);

    portENTER_CRITICAL_ISR(&CANmodule->state_lock);
    uint16_t COError = CANmodule->CANerrorStatus;
    COError &= ~(CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE);
    switch (edata->new_sta) {
    case TWAI_ERROR_WARNING:
        COError |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRRX_WARNING;
        break;
    case TWAI_ERROR_PASSIVE:
        COError |= CO_CAN_ERRTX_PASSIVE | CO_CAN_ERRRX_PASSIVE;
        break;
    case TWAI_ERROR_BUS_OFF:
        COError |= CO_CAN_ERRTX_BUS_OFF;
        break;
    default:
        break;
    }
    CANmodule->CANerrorStatus = COError;
    portEXIT_CRITICAL_ISR(&CANmodule->state_lock);

    return true;
}

void CO_CANsetConfigurationMode(void *CANptr)
{
    (void)CANptr;   // Not used, because this function is called before `CO_CANmodule_init`
}

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, void *CANptr, CO_CANrx_t rxArray[], uint16_t rxSize,
                                   CO_CANtx_t txArray[], uint16_t txSize, uint16_t CANbitRate)
{
    /* TWAI bitrate/configuration is owned by the application layer. */
    (void)CANbitRate;
    ESP_RETURN_ON_FALSE(CANmodule && CANptr && rxArray && txArray, CO_ERROR_ILLEGAL_ARGUMENT, TAG, "CO_CANmodule_init invalid args");

    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0U;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->CANtxCount = 0U;
    portMUX_INITIALIZE(&CANmodule->state_lock);
    portMUX_INITIALIZE(&CANmodule->co_lock);
    _lock_init(&CANmodule->tx_mutex);

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
        .on_state_change = co_twai_state_callback,
    };
    ESP_RETURN_ON_FALSE(ESP_OK == twai_node_register_event_callbacks((twai_node_handle_t)CANptr, &cbs, CANmodule), CO_ERROR_SYSCALL, TAG, "CO_CANmodule_init twai_node_register_event_callbacks failed");

    return CO_ERROR_NO;
}

void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    ESP_RETURN_ON_FALSE(CANmodule && CANmodule->CANptr,, TAG, "CO_CANsetNormalMode invalid args");

    ESP_RETURN_ON_FALSE(ESP_OK == twai_node_enable((twai_node_handle_t)CANmodule->CANptr),, TAG, "CO_CANsetNormalMode twai_node_enable failed");
    CANmodule->CANnormal = true;
}

void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    ESP_RETURN_ON_FALSE(CANmodule,, TAG, "CO_CANmodule_disable invalid args");

    // In standard flow, CO_CANmodule_disable is able to be called before CO_CANmodule_init
    if (CANmodule->CANptr) {
        ESP_RETURN_ON_FALSE(ESP_OK == twai_node_disable((twai_node_handle_t)CANmodule->CANptr),, TAG, "CO_CANmodule_disable twai_node_disable failed");
    }
    CANmodule->CANnormal = false;
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr,
                                    void *object, void (*CANrx_callback)(void *object, void *message))
{
    ESP_RETURN_ON_FALSE(CANmodule && index < CANmodule->rxSize, CO_ERROR_ILLEGAL_ARGUMENT, TAG, "CO_CANrxBufferInit invalid args");

    CO_CANrx_t *buffer = &CANmodule->rxArray[index];
    buffer->object = object;
    buffer->CANrx_callback = CANrx_callback;
    buffer->ident = (ident & TWAI_STD_ID_MASK) | (rtr ? CO_ID_FLAG_RTR : 0U);
    buffer->mask = (mask & TWAI_STD_ID_MASK) | CO_ID_FLAG_RTR;
    return CO_ERROR_NO;
}

CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                               bool_t syncFlag)
{
    ESP_RETURN_ON_FALSE(CANmodule && index < CANmodule->txSize && noOfBytes <= 8U, NULL, TAG, "CO_CANtxBufferInit invalid args");

    CO_CANtx_t *buffer = &CANmodule->txArray[index];
    buffer->tx_frame.header.id = ident & TWAI_STD_ID_MASK;
    buffer->tx_frame.header.ide = false;
    buffer->tx_frame.header.rtr = rtr;
    buffer->tx_frame.header.dlc = noOfBytes;
    buffer->tx_frame.buffer = buffer->data;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    ESP_RETURN_ON_FALSE(CANmodule && buffer, CO_ERROR_ILLEGAL_ARGUMENT, TAG, "CO_CANsend invalid args");

    // Try to transmit immediately. If failed, queue the frame in the CANopenNode
    // software buffer for later retry by CO_CANmodule_process.
    ESP_LOGD(TAG, "Tx %3lx [%d] %x %x %x %x %x %x %x %x", buffer->tx_frame.header.id, buffer->tx_frame.header.dlc, buffer->tx_frame.buffer[0], buffer->tx_frame.buffer[1], buffer->tx_frame.buffer[2], buffer->tx_frame.buffer[3], buffer->tx_frame.buffer[4], buffer->tx_frame.buffer[5], buffer->tx_frame.buffer[6], buffer->tx_frame.buffer[7]);
    _lock_acquire(&CANmodule->tx_mutex);
    if (ESP_OK != twai_node_transmit((twai_node_handle_t)CANmodule->CANptr, &buffer->tx_frame, 0U)) {
        if (!buffer->bufferFull) {
            CANmodule->CANtxCount++;
        }
        buffer->bufferFull = true;
        _lock_release(&CANmodule->tx_mutex);
        return CO_ERROR_TX_BUSY;
    }
    if (CANmodule->CANtxCount > 0U) {
        CANmodule->CANtxCount--;
    }
    buffer->bufferFull = false;
    CANmodule->bufferInhibitFlag = buffer->syncFlag;
    _lock_release(&CANmodule->tx_mutex);

    return CO_ERROR_NO;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    ESP_RETURN_ON_FALSE(CANmodule,, TAG, "CO_CANclearPendingSyncPDOs invalid args");

    bool_t tpdoDeleted = false;
    _lock_acquire(&CANmodule->tx_mutex);
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
    _lock_release(&CANmodule->tx_mutex);

    if (tpdoDeleted) {
        portENTER_CRITICAL(&CANmodule->state_lock);
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
        portEXIT_CRITICAL(&CANmodule->state_lock);
    }
}

void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    if (!CANmodule || CANmodule->CANtxCount <= 0U) {
        return;
    }

    for (uint16_t i = 0U; i < CANmodule->txSize; i++) {
        CO_CANtx_t *buffer = &CANmodule->txArray[i];
        if (!buffer->bufferFull) {
            continue;
        }
        /* Respect inhibit flag for SYNC-related frames, as in standard CANopenNode. */
        if (CANmodule->bufferInhibitFlag && buffer->syncFlag) {
            continue;
        }
        CO_CANsend(CANmodule, buffer);
    }
}
