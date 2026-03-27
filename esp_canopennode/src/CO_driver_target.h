/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

/* ESP32 specific CANopenNode target declarations.
 * This file intentionally stays framework-only for now. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic definitions for ESP32 (little endian). */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

/* Minimal RX message representation used by callback dispatch path. */
typedef struct {
    uint16_t ident;
    uint8_t DLC;
    uint8_t data[8];
} CO_CANrxMsg_t;

/* Accessors used by CANopenNode internals. */
#define CO_CANrxMsg_readIdent(msg) (((const CO_CANrxMsg_t *)(msg))->ident)
#define CO_CANrxMsg_readDLC(msg)   (((const CO_CANrxMsg_t *)(msg))->DLC)
#define CO_CANrxMsg_readData(msg)  (((const CO_CANrxMsg_t *)(msg))->data)

/* RX message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* TX message object */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
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
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
} CO_CANmodule_t;

/* Critical section placeholders for framework stage. */
#define CO_LOCK_CAN_SEND(CAN_MODULE)
#define CO_UNLOCK_CAN_SEND(CAN_MODULE)
#define CO_LOCK_EMCY(CAN_MODULE)
#define CO_UNLOCK_EMCY(CAN_MODULE)
#define CO_LOCK_OD(CAN_MODULE)
#define CO_UNLOCK_OD(CAN_MODULE)

/* Synchronization helpers */
#define CO_MemoryBarrier() __sync_synchronize()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                                                                                             \
    {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = (void *)1L;                                                                                            \
    }
#define CO_FLAG_CLEAR(rxNew)                                                                                           \
    {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = NULL;                                                                                                  \
    }

#ifdef __cplusplus
}
#endif

#endif /* CO_DRIVER_TARGET_H */
