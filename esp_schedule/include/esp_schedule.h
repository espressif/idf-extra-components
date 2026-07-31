/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_schedule.h
 * @brief The complete public interface of the esp_schedule component.
 *
 * Two halves, in this order: the scheduling API, then the porting interface.
 *
 * esp_schedule reaches the outside world - timers, storage, wall-clock time, the
 * heap and the log - only through the function pointers in the second half.
 * esp_schedule_init() installs the ESP-IDF implementations of all of them;
 * esp_schedule_init_with_config() lets an integrator supply their own. Both are
 * declared here, so there is one place to compare them.
 *
 * @note Anything that only calls esp_schedule_init_with_config() never
 *       references esp_schedule_init(), which is the sole referrer of the
 *       ESP-IDF implementations. They therefore stay out of the link entirely -
 *       no FreeRTOS timer, nvs_flash or esp_log dependency is pulled in on
 *       behalf of this component. See src/esp_schedule_default.c.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Schedule Handle */
typedef void *esp_schedule_handle_t;

/** Maximum length of the schedule name allowed. This value cannot be more than 16 as it is used for NVS key. */
#define MAX_SCHEDULE_NAME_LEN 16

/** Callback for schedule trigger
 *
 * This callback is called when the schedule is triggered.
 *
 * @param[in] handle Schedule handle.
 * @param[in] priv_data Pointer to the private data passed while creating/editing the schedule.
 */
typedef void (*esp_schedule_trigger_cb_t)(esp_schedule_handle_t handle, void *priv_data);

/** Callback for schedule timestamp
 *
 * This callback is called when the next trigger timestamp of the schedule is changed. This might be useful to check if
 * one time schedules have already passed while the device was powered off.
 *
 * @param[in] handle Schedule handle.
 * @param[in] next_timestamp timestamp at which the schedule will trigger next.
 * @param[in] priv_data Pointer to the user data passed while creating/editing the schedule.
 */
typedef void (*esp_schedule_timestamp_cb_t)(esp_schedule_handle_t handle, uint32_t next_timestamp, void *priv_data);

/** Schedule type */
typedef enum esp_schedule_type {
    ESP_SCHEDULE_TYPE_INVALID = 0,
    ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    ESP_SCHEDULE_TYPE_DATE,
    ESP_SCHEDULE_TYPE_RELATIVE,
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    ESP_SCHEDULE_TYPE_SUNRISE,
    ESP_SCHEDULE_TYPE_SUNSET,
#endif
} esp_schedule_type_t;

/** Schedule days. Used for ESP_SCHEDULE_TYPE_DAYS_OF_WEEK. */
typedef enum esp_schedule_days {
    ESP_SCHEDULE_DAY_ONCE      = 0,
    ESP_SCHEDULE_DAY_EVERYDAY  = 0b1111111,
    ESP_SCHEDULE_DAY_MONDAY    = 1 << 0,
    ESP_SCHEDULE_DAY_TUESDAY   = 1 << 1,
    ESP_SCHEDULE_DAY_WEDNESDAY = 1 << 2,
    ESP_SCHEDULE_DAY_THURSDAY  = 1 << 3,
    ESP_SCHEDULE_DAY_FRIDAY    = 1 << 4,
    ESP_SCHEDULE_DAY_SATURDAY  = 1 << 5,
    ESP_SCHEDULE_DAY_SUNDAY    = 1 << 6,
} esp_schedule_days_t;

/** Schedule months. Used for ESP_SCHEDULE_TYPE_DATE. */
typedef enum esp_schedule_months {
    ESP_SCHEDULE_MONTH_ONCE         = 0,
    ESP_SCHEDULE_MONTH_ALL          = 0b111111111111,
    ESP_SCHEDULE_MONTH_JANUARY      = 1 << 0,
    ESP_SCHEDULE_MONTH_FEBRUARY     = 1 << 1,
    ESP_SCHEDULE_MONTH_MARCH        = 1 << 2,
    ESP_SCHEDULE_MONTH_APRIL        = 1 << 3,
    ESP_SCHEDULE_MONTH_MAY          = 1 << 4,
    ESP_SCHEDULE_MONTH_JUNE         = 1 << 5,
    ESP_SCHEDULE_MONTH_JULY         = 1 << 6,
    ESP_SCHEDULE_MONTH_AUGUST       = 1 << 7,
    ESP_SCHEDULE_MONTH_SEPTEMBER    = 1 << 8,
    ESP_SCHEDULE_MONTH_OCTOBER      = 1 << 9,
    ESP_SCHEDULE_MONTH_NOVEMBER     = 1 << 10,
    ESP_SCHEDULE_MONTH_DECEMBER     = 1 << 11,
} esp_schedule_months_t;

/** Trigger details of the schedule */
typedef struct esp_schedule_trigger {
    /** Type of schedule */
    esp_schedule_type_t type;
    /** Hours in 24 hour format. Accepted values: 0-23 */
    uint8_t hours;
    /** Minutes in the given hour. Accepted values: 0-59. */
    uint8_t minutes;
    /** For type ESP_SCHEDULE_TYPE_DAYS_OF_WEEK and solar schedules with day-of-week patterns */
    struct {
        /** 'OR' list of esp_schedule_days_t */
        uint8_t repeat_days;
    } day;
    /** For type ESP_SCHEDULE_TYPE_DATE and solar schedules with specific date patterns */
    struct {
        /** Day of the month. Accepted values: 1-31. */
        uint8_t day;
        /* 'OR' list of esp_schedule_months_t */
        uint16_t repeat_months;
        /** Year */
        uint16_t year;
        /** If the schedule is to be repeated every year. */
        bool repeat_every_year;
    } date;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    /** For type ESP_SCHEDULE_TYPE_SUNRISE and ESP_SCHEDULE_TYPE_SUNSET
     * Uses day.repeat_days for day-of-week patterns (if date.day == 0)
     * Uses date.* fields for specific date patterns (if date.day != 0)
     * If both are 0, treated as single-time schedule */
    struct {
        /** Latitude in decimal degrees (-90 to +90, positive North) */
        double latitude;
        /** Longitude in decimal degrees (-180 to +180, positive East) */
        double longitude;
        /** Offset in minutes from sunrise/sunset (positive = after, negative = before) */
        int offset_minutes;
    } solar;
#endif
    /** For type ESP_SCHEDULE_TYPE_SECONDS */
    int relative_seconds;
    /** Used for passing the next schedule timestamp for
     * ESP_SCHEDULE_TYPE_RELATIVE */
    time_t next_scheduled_time_utc;
} esp_schedule_trigger_t;

/** Schedule Validity
 * Start and end time within which the schedule will be applicable.
 */
typedef struct esp_schedule_validity {
    /* Start time as UTC timestamp */
    time_t start_time;
    /* End time as UTC timestamp */
    time_t end_time;
} esp_schedule_validity_t;

/** Schedule config */
typedef struct esp_schedule_config {
    /** Name of the schedule. This is like a primary key for the schedule. This is required. +1 for NULL termination. */
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    /** Trigger details */
    esp_schedule_trigger_t trigger;
    /** Trigger callback */
    esp_schedule_trigger_cb_t trigger_cb;
    /** Timestamp callback */
    esp_schedule_timestamp_cb_t timestamp_cb;
    /** Private data associated with the schedule. This will be passed to callbacks. */
    void *priv_data;
    /** Validity of schedules. */
    esp_schedule_validity_t validity;
} esp_schedule_config_t;

/** Initialize ESP Schedule
 *
 * This initializes ESP Schedule. This must be called first before calling any of the other APIs.
 * This API also gets all the schedules from NVS (if it has been enabled).
 *
 * @warning After calling this API, the pointers to the callbacks should be updated for all the schedules by calling
 * esp_schedule_get() followed by esp_schedule_edit() with the correct callbacks.
 *
 * @param[in] enable_nvs If NVS is to be enabled or not.
 * @param[in] nvs_partition (Optional) The NVS partition to be used. If NULL is passed, the default partition is used.
 * @param[out] schedule_count Number of active schedules found in NVS.
 *
 * @return Array of schedule handles if any schedules have been found.
 * @return NULL if no schedule is found in NVS (or if NVS is not enabled).
 */
esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count);

/** Create Schedule
 *
 * This API can be used to create a new schedule. The schedule still needs to be enabled using
 * esp_schedule_enable().
 *
 * @param[in] schedule_config Configuration of the schedule to be created.
 *
 * @return Schedule handle if successfully created.
 * @return NULL in case of error.
 */
esp_schedule_handle_t esp_schedule_create(esp_schedule_config_t *schedule_config);

/** Remove Schedule
 *
 * This API can be used to remove an existing schedule.
 *
 * @param[in] handle Schedule handle for the schedule to be removed.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_delete(esp_schedule_handle_t handle);

/** Edit Schedule
 *
 * This API can be used to edit an existing schedule.
 * The schedule name should be same as when the schedule was created. The complete config must be provided
 * or the previously stored config might be over-written.
 *
 * Note: If a schedule is edited when it is on-going, the new changes will not be reflected.
 * You will need to disable the schedule, edit it, and then enable it again.
 *
 * @param[in] handle Schedule handle for the schedule to be edited.
 * @param[in] schedule_config Configuration of the schedule to be edited.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

/** Enable Schedule
 *
 * This API can be used to enable an existing schedule.
 * It can be used to enable a schedule after it has been created using esp_schedule_create()
 * or if the schedule has been disabled using esp_schedule_disable().
 *
 * @param[in] handle Schedule handle for the schedule to be enabled.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_enable(esp_schedule_handle_t handle);

/** Disable Schedule
 *
 * This API can be used to disable an on-going schedule.
 * It does not remove the schedule, just stops it. The schedule can be enabled again using
 * esp_schedule_enable().
 *
 * @param[in] handle Schedule handle for the schedule to be disabled.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_disable(esp_schedule_handle_t handle);

/** Get Schedule
 *
 * This API can be used to get details of an existing schedule.
 * The schedule_config is populated with the schedule details.
 *
 * @param[in] handle Schedule handle.
 * @param[out] schedule_config Details of the schedule whose handle is passed.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

/* Timer **********************************************************************/

/** Opaque timer handle owned by the timer implementation. */
typedef void *esp_schedule_timer_handle_t;

/** Called by the timer implementation when a timer expires.
 *
 * @param[in] priv_data Private data given to esp_schedule_timer_ops_t::start.
 */
typedef void (*esp_schedule_timer_cb_t)(void *priv_data);

/** One-shot timer operations. All members are required.
 *
 * @note Callback bodies must be serialized against each other: at most one
 *       esp_schedule_timer_cb_t may be executing at any time, across all
 *       timers. esp_schedule relies on this to detect a schedule deleting
 *       itself from its own trigger callback, where the deletion has to be
 *       deferred until the callback returns. A port that dispatched two
 *       schedules concurrently on two tasks would defeat that detection and
 *       free a schedule while its callback is still running.
 *
 *       Serializing under a single mutex held for the whole callback satisfies
 *       both this and the @c cancel barrier below. It must be recursive, or
 *       taken in a way that tolerates re-entry, because the callback may call
 *       back into @c start or @c cancel on the same task.
 */
typedef struct esp_schedule_timer_ops {
    /** Arm a one-shot timer for @c delay_seconds.
     *
     * Creates the timer on the first call for a given handle and reuses it on
     * later calls. @c cb and @c priv_data are (re)bound on every call.
     *
     * The implementation must support delays longer than its native period can
     * represent, and must tolerate being called from within @c cb.
     *
     * @param[in,out] p_timer_handle NULL-valued on first use; set to the created
     *                               handle and reused thereafter.
     * @return true if the timer is armed, false otherwise.
     */
    bool (*start)(esp_schedule_timer_handle_t *p_timer_handle, uint32_t delay_seconds,
                  esp_schedule_timer_cb_t cb, void *priv_data);

    /** Stop a timer without destroying it. Must tolerate a NULL handle. */
    void (*stop)(esp_schedule_timer_handle_t timer_handle);

    /** Stop and destroy a timer, then set @c *p_timer_handle to NULL.
     *
     * Must not return while a callback is still executing, and must tolerate
     * being called from within that callback.
     */
    void (*cancel)(esp_schedule_timer_handle_t *p_timer_handle);
} esp_schedule_timer_ops_t;

/* Storage ********************************************************************/

/** Opaque storage handle owned by the storage implementation. */
typedef void *esp_schedule_store_handle_t;

/** Callback for esp_schedule_nvs_ops_t::foreach_key.
 *
 * @return true to continue iterating, false to stop early.
 */
typedef bool (*esp_schedule_key_cb_t)(const char *key, void *ctx);

/** Key/value storage operations.
 *
 * Leave every member NULL to build without persistence; schedules then live
 * only for the lifetime of the process. If any member is set they must all be.
 */
typedef struct esp_schedule_nvs_ops {
    /** Open @c name_space in @c partition. @c partition may be NULL for the
     * implementation's default. */
    esp_err_t (*open)(const char *partition, const char *name_space, bool readonly,
                      esp_schedule_store_handle_t *p_handle);

    /** Flush any pending writes and release the handle.
     *
     * @note This is the commit point. Writes made since open() need not be
     *       durable until close() returns, which lets an implementation batch
     *       them; esp_schedule never relies on a partial batch being visible.
     */
    void (*close)(esp_schedule_store_handle_t handle);

    /** Read the value of @c key.
     *
     * With @c value NULL, set @c *p_value_len to the stored size and return
     * ESP_OK. Otherwise read at most @c *p_value_len bytes and update it to the
     * number read.
     *
     * @return ESP_ERR_NVS_NOT_FOUND (or ESP_ERR_NOT_FOUND) if @c key is absent.
     */
    esp_err_t (*read)(esp_schedule_store_handle_t handle, const char *key,
                      void *value, size_t *p_value_len);

    /** Write @c value_len bytes under @c key, replacing any existing value. */
    esp_err_t (*write)(esp_schedule_store_handle_t handle, const char *key,
                       const void *value, size_t value_len);

    /** Erase @c key, or every key in the namespace when @c key is NULL. */
    esp_err_t (*erase)(esp_schedule_store_handle_t handle, const char *key);

    /** Invoke @c cb once per key in the namespace.
     *
     * Takes a partition/namespace rather than an open handle so the
     * implementation is free to use whatever iteration primitive it has. The
     * key passed to @c cb is owned by the implementation and need only stay
     * valid for the duration of the call, which keeps iteration allocation-free.
     */
    esp_err_t (*foreach_key)(const char *partition, const char *name_space,
                             esp_schedule_key_cb_t cb, void *ctx);
} esp_schedule_nvs_ops_t;

/* Time synchronization *******************************************************/

/** Time synchronization operations. @c get_time is required. */
typedef struct esp_schedule_time_sync_ops {
    /** Current UTC time. Also stored to @c p_time when that is non-NULL. */
    time_t (*get_time)(time_t *p_time);

    /** Start time synchronisation, if the platform has any. May be NULL. */
    void (*timesync_init)(void);
} esp_schedule_time_sync_ops_t;

/* Memory *********************************************************************/

/** Heap operations. All members are required. */
typedef struct esp_schedule_mem_ops {
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t num, size_t size);
    void (*free)(void *ptr);
} esp_schedule_mem_ops_t;

/* Log ************************************************************************/

/** Log severity, ordered from most to least severe. */
typedef enum esp_schedule_log_level {
    ESP_SCHEDULE_LOG_ERROR = 0,
    ESP_SCHEDULE_LOG_WARN,
    ESP_SCHEDULE_LOG_INFO,
    ESP_SCHEDULE_LOG_DEBUG,
    ESP_SCHEDULE_LOG_VERBOSE,
} esp_schedule_log_level_t;

/** Emit one already-formatted log line. May be NULL, which discards all output.
 *
 * @c message is NUL-terminated and owned by the caller; it is only valid for
 * the duration of the call, so an implementation that defers must copy it.
 *
 * Formatting happens before this is called, so a port needs no varargs printf
 * of its own. It also keeps va_list handling entirely inside the component:
 * were the list passed across this boundary, an implementation that called
 * va_end() on it would double-end the caller's list, which is undefined.
 *
 * Calls above the compile-time ceiling are removed by the preprocessor and
 * never reach this function. The ceiling is CONFIG_LOG_MAXIMUM_LEVEL, or
 * CONFIG_ESP_SCHEDULE_LOG_LEVEL when CONFIG_ESP_SCHEDULE_LOG_LEVEL_OVERRIDE is
 * enabled.
 */
typedef void (*esp_schedule_log_fn_t)(esp_schedule_log_level_t level, const char *tag,
                                      const char *message);

/* Port configuration *********************************************************/

/** The complete set of operations esp_schedule needs from its environment.
 *
 * Copied by value during init, so a caller may build it on the stack.
 */
typedef struct esp_schedule_port_config {
    esp_schedule_timer_ops_t timer;          /**< Required. */
    esp_schedule_nvs_ops_t nvs;              /**< Optional; all-NULL disables persistence. */
    esp_schedule_time_sync_ops_t time_sync;  /**< Required, but only get_time within it. */
    esp_schedule_mem_ops_t mem;              /**< Required. */
    esp_schedule_log_fn_t log;               /**< Optional; NULL discards all output. */
} esp_schedule_port_config_t;

/**
 * @brief Initialize ESP Schedule against a caller-supplied port.
 *
 * Behaves exactly like esp_schedule_init(), except that the platform
 * operations come from @c port instead of the built-in ESP-IDF ones. Using
 * this entry point in place of esp_schedule_init() keeps the ESP-IDF
 * implementations out of the binary.
 *
 * @param[in] port Operations to install. Copied; need not outlive the call.
 *                 Persistence is unavailable if @c port->nvs is all-NULL,
 *                 whatever @c enable_nvs says.
 * @param[in] enable_nvs Whether to enable persistence.
 * @param[in] nvs_partition (Optional) Partition to use, NULL for the default.
 * @param[out] schedule_count Number of schedules restored from storage.
 *
 * @return Array of restored schedule handles, or NULL if there are none.
 *
 * @note The caller owns the returned array and must release it with the same
 *       allocator it supplied - @c port->mem.free, not necessarily libc
 *       free(). The array is allocated with @c port->mem.malloc. Only the array
 *       itself is the caller's; the handles in it stay owned by the component.
 */
esp_schedule_handle_t *esp_schedule_init_with_config(const esp_schedule_port_config_t *port,
        bool enable_nvs, char *nvs_partition,
        uint8_t *schedule_count);

#ifdef __cplusplus
}
#endif
