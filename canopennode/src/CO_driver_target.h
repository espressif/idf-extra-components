/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/* ESP Chips specific CANopenNode target declarations.
 * This file intentionally stays framework-only for now. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_twai.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_CO_SDO_CLIENT
#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE|CO_CONFIG_SDO_CLI_SEGMENTED|CO_CONFIG_SDO_CLI_LOCAL)
#define CO_CONFIG_FIFO (CO_CONFIG_FIFO_ENABLE)
#endif

// Following modules are not supported in this driver yet
#define CO_CONFIG_LEDS  0
#define CO_CONFIG_LSS   0
#define CO_CONFIG_HB_CONS 0
#define CO_CONFIG_TIME 0
#define CO_CONFIG_SYNC 0

#ifdef CONFIG_CO_PDO
/* Enable asynchronous PDOs only. Because SYNC not supported yet. */
#define CO_CONFIG_PDO                                                                                                    \
    (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE | CO_CONFIG_RPDO_TIMERS_ENABLE | CO_CONFIG_TPDO_TIMERS_ENABLE       \
     | CO_CONFIG_PDO_OD_IO_ACCESS | CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT            \
     | CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#else
#define CO_CONFIG_PDO 0
#endif

#ifdef CONFIG_CO_MULTIPLE_OD
#define CO_MULTIPLE_OD
#endif

/* Basic definitions for ESP Chips (little endian). */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) (x)
#define CO_SWAP_32(x) (x)
#define CO_SWAP_64(x) (x)

typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

#define CO_ID_FLAG_RTR   0x8000U /*!< RTR flag, part of identifier */

/* Accessors used by CANopenNode internals. */
#define CO_CANrxMsg_readIdent(msg) (((const twai_frame_t *)(msg))->header.id & ~CO_ID_FLAG_RTR)
#define CO_CANrxMsg_readDLC(msg)   (((const twai_frame_t *)(msg))->header.dlc)
#define CO_CANrxMsg_readData(msg)  (((const twai_frame_t *)(msg))->buffer)

/* RX message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* TX message object */
typedef struct {
    twai_frame_t tx_frame;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object */
typedef struct {
    void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile uint16_t CANtxCount;
    portMUX_TYPE state_lock;
    portMUX_TYPE co_lock;
    _lock_t tx_mutex;
} CO_CANmodule_t;

/* Critical section implementation using ESP-IDF FreeRTOS primitives. */
#define CO_LOCK_CAN_SEND(CAN_MODULE)   portENTER_CRITICAL(&CAN_MODULE->co_lock)
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) portEXIT_CRITICAL(&CAN_MODULE->co_lock)
#define CO_LOCK_EMCY(CAN_MODULE)       portENTER_CRITICAL(&CAN_MODULE->co_lock)
#define CO_UNLOCK_EMCY(CAN_MODULE)     portEXIT_CRITICAL(&CAN_MODULE->co_lock)
#define CO_LOCK_OD(CAN_MODULE)         portENTER_CRITICAL(&CAN_MODULE->co_lock)
#define CO_UNLOCK_OD(CAN_MODULE)       portEXIT_CRITICAL(&CAN_MODULE->co_lock)

/* Synchronization helpers */
#define CO_MemoryBarrier() __sync_synchronize()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                                                                                             \
    do {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = (void *)1L;                                                                                            \
    } while (0)
#define CO_FLAG_CLEAR(rxNew)                                                                                           \
    do {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = NULL;                                                                                                  \
    } while (0)

#ifdef __cplusplus
}
#endif
