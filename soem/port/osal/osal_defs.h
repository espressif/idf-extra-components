/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the LICENSE file distributed with this software for
 * full license information.
 */

#ifndef ESP_SOEM_OSAL_DEFS_H
#define ESP_SOEM_OSAL_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef EC_DEBUG
#define EC_PRINT(...) printf(__VA_ARGS__) /* print debug info */
#else
#define EC_PRINT(...) \
    do {              \
    } while (0)
#endif

/* EtherCAT protocol structures must not contain compiler-inserted padding. */
#ifndef OSAL_PACKED
#define OSAL_PACKED_BEGIN
#define OSAL_PACKED __attribute__((packed))
#define OSAL_PACKED_END
#endif

/* Map SOEM time type to esp-idf timespec, both in (s, ns). */
#define ec_timet struct timespec

/* Map SOEM thread abstractions to FreeRTOS tasks. */
#define OSAL_THREAD_HANDLE TaskHandle_t
#define OSAL_THREAD_FUNC void
#define OSAL_THREAD_FUNC_RT void

/* Map SOEM mutual exclusion type to FreeRTOS semaphore handle. */
#define osal_mutext SemaphoreHandle_t

#ifdef __cplusplus
}
#endif

#endif
