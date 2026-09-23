/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the LICENSE file distributed with this software for
 * full license information.
 */

#include "osal.h"
#include <stdlib.h>
#include <sys/time.h>
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Time format conversion, from us to (s, ns).
   Same as osal_timespec_from_usec macro. */
static void usec_to_timespec(int64_t usec, ec_timet *ts)
{
    ts->tv_sec = (time_t)(usec / 1000000);
    ts->tv_nsec = (long)((usec % 1000000) * 1000);
}

/* Time format conversion, from (s, ns) to us. */
static int64_t timespec_to_usec(const ec_timet *ts)
{
    return (int64_t)ts->tv_sec * 1000000 +
           (int64_t)ts->tv_nsec / 1000;
}

/* Get monotonic time in (s, ns) format.
   SOEM timeout handling requires time since system startup. */
void osal_get_monotonic_time(ec_timet *ts)
{
    usec_to_timespec(esp_timer_get_time(), ts);
}

/* Get current time(wall-clock/real-time) in (s, ns) format.
   Used for SOEM logging timestamp and DC initialization. */
ec_timet osal_current_time(void)
{
    struct timeval tv;
    ec_timet ts = {0};

    if (gettimeofday(&tv, NULL) == 0) {
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = (long)tv.tv_usec * 1000;
    }

    return ts;
}

/* Calculate the time difference between two SOEM time points.
   Computes (end - start) and stores the result in diff. */
void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff)
{
    osal_timespecsub(end, start, diff); /* macro. */
}

/* Set an expiration timepoint in (s, ns) format.
   Use the current monotonic time as base. Convert timeout duration(us) to (s, ns) and add it.
   Store the result in self->stop_time. */
void osal_timer_start(osal_timert *self, uint32 timeout_usec)
{
    ec_timet now;
    ec_timet timeout;

    osal_get_monotonic_time(&now);
    osal_timespec_from_usec(timeout_usec, &timeout); /* macro, use static usec_to_timespec() is also OK. */
    osal_timespecadd(&now, &timeout, &self->stop_time); /* macro. */
}

/* Check if the expiration timepoint saved in self->stop_time has been reached.
   Return TRUE if expired(include '='), FALSE otherwise. */
boolean osal_timer_is_expired(osal_timert *self)
{
    ec_timet now;

    osal_get_monotonic_time(&now);
    return osal_timespeccmp(&now, &self->stop_time, >=) ? TRUE : FALSE; /* macro. */
}

/* Sleep until a specific absolute timepoint(deadline) given by @param ts.
   Internally convert into us and FreeRTOS tick number.
   Use FreeRTOS blocking delay for whole ticks and busy-wait for remaining sub-tick interval.
   Each busy-wait step limited to max 1ms, will re-read monotonic time. */
int osal_monotonic_sleep(ec_timet *ts)
{
    const int64_t target_usec = timespec_to_usec(ts);
    const int64_t tick_usec = 1000000LL / configTICK_RATE_HZ;

    for (;;) {
        int64_t remaining_usec = target_usec - esp_timer_get_time();

        if (remaining_usec <= 0) {
            return 0;
        }

        if (remaining_usec >= tick_usec) {
            TickType_t ticks = (TickType_t)(remaining_usec / tick_usec);
            vTaskDelay(ticks);
            continue;
        }

        uint32_t busy_usec = (remaining_usec > 1000)
                             ? 1000 : (uint32_t)remaining_usec;
        esp_rom_delay_us(busy_usec);
    }
}

/* Delay the current task for the specified number of us given by @param usec.
   Calculate the absolute deadline timepoint based on current monotonic time,
   then call osal_monotonic_sleep().
   Not a hard real-time delay, actual wake-up timepoint may be later than expected,
   due to FreeRTOS scheduling, other task preemption and interrupt handling. */
int osal_usleep(uint32 usec)
{
    ec_timet deadline;

    usec_to_timespec(esp_timer_get_time() + (int64_t)usec, &deadline);
    return osal_monotonic_sleep(&deadline);
}

/* Allocate memory on the heap. */
void *osal_malloc(size_t size)
{
    return malloc(size);
}

/* Free memory on the heap. */
void osal_free(void *ptr)
{
    free(ptr);
}

/* Create a FreeRTOS task for SOEM thread abstraction.
   @param thandle, Pointer to storage for the created FreeRTOS task handle.
   @param stacksize, ESP-IDF requested stack size in bytes, defaults to 4096B.
   @param func, Pointer to the task entry function.
   @param param, Argument passed to the task entry function.
   @param priority, FreeRTOS task priority.
   @param name, FreeRTOS task name used for debugging.
*/
static int osal_task_create(void *thandle, int stacksize, void *func,
                            void *param, UBaseType_t priority, const char *name)
{
    TaskHandle_t *handle = (TaskHandle_t *)thandle;
    TaskFunction_t entry = (TaskFunction_t)func;
    uint32_t stack_bytes = (stacksize > 0) ? (uint32_t)stacksize : 4096U;

    if (handle == NULL || entry == NULL) {
        return 0;
    }

    BaseType_t result = xTaskCreate(entry, name, stack_bytes, param, priority, handle);

    return result == pdPASS ? 1 : 0;
}

/* Create an ordinary SOEM background task with default priority 5.
   If default priority exceeds the highest possible value, set to highest.
   FreeRTOS legal priority range is 0 to (configMAX_PRIORITIES - 1). */
int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
    UBaseType_t priority = 5; /* ESP-IDF port own strategy, can be customized. */

    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
    }

    return osal_task_create(thandle, stacksize, func, param, priority, "soem_worker");
}

/* Create a higher-priority SOEM background task with the second highest priority. */
int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
    UBaseType_t priority = (configMAX_PRIORITIES > 2)
                           ? configMAX_PRIORITIES - 2
                           : configMAX_PRIORITIES - 1; /* exception: only 1 priority level. */

    return osal_task_create(thandle, stacksize, func, param, priority, "soem_worker_rt");
}

/* Create a FreeRTOS mutex for SOEM mutual exclusion.
   Notice FreeRTOS mutexes use the semaphore API also. */
void *osal_mutex_create(void)
{
    return (void *)xSemaphoreCreateMutex();
}

/* Destroy a FreeRTOS mutex. Check in case non-existent mutex cause crash. */
void osal_mutex_destroy(void *mutex)
{
    if (mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}

/* Lock a FreeRTOS mutex, blocking wait forever until it becomes available. */
void osal_mutex_lock(void *mutex)
{
    if (mutex != NULL) {
        (void)xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY);
    }
}

/* Unlock a previously acquired FreeRTOS mutex. */
void osal_mutex_unlock(void *mutex)
{
    if (mutex != NULL) {
        (void)xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}
