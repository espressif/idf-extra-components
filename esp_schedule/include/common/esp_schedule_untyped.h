/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_schedule_untyped.h
 * @brief Untyped interface for the esp_schedule component
 *
 * This interface is used to create and manage schedules without specifying the return type.
 * This is useful for cases where the return type is build-specific (e.g., esp_err_t or something else).
 * @note You should not use this header file directly. Instead include this header file, and define the following:
 * - ESP_SCHEDULE_RETURN_TYPE to the return type you want to use.
 * - The following return values MUST be defined:
 *   - ESP_SCHEDULE_RET_OK           : Success.
 *   - ESP_SCHEDULE_RET_FAIL         : Failure.
 *   - ESP_SCHEDULE_RET_NO_MEM       : No memory.
 *   - ESP_SCHEDULE_RET_INVALID_ARG  : Invalid argument.
 *   - ESP_SCHEDULE_RET_INVALID_STATE: Invalid state.
 *   - ESP_SCHEDULE_RET_NVS_NOT_FOUND: NVS not found.
 */

#ifndef __ESP_SCHEDULE_UNTYPED_H__
#define __ESP_SCHEDULE_UNTYPED_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

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

#if defined(CONFIG_ESP_SCHEDULE_ENABLE_NVS) && CONFIG_ESP_SCHEDULE_ENABLE_NVS
/** Callback for saving private data
 *
 * This callback is called when the private data is to be saved to NVS.
 * Translate the private data to a binary format and save it to the data pointer.
 * @note if p_data is NULL, then return the required size of the data buffer in p_data_len.
 * @param[in] priv_data Pointer to the private data to be saved.
 * @param[out] p_data Pointer to the data to be saved. This is a binary buffer. This will be freed by the caller.
 * @param[out] p_data_len Pointer to the length of the data to be saved.
 */
typedef void (*esp_schedule_priv_data_save_cb_t)(void *priv_data, void **p_data, size_t *p_data_len);

/** Callback for loading private data
 *
 * This callback is called when the private data is to be loaded from NVS.
 * Translate the binary data to the private data.
 *
 * @param[in] data Pointer to the data to be loaded. This will be freed by the caller.
 * @param[in] data_len Length of the data to be loaded.
 * @param[out] p_priv_data Pointer to the private data to be loaded.
 */
typedef void (*esp_schedule_priv_data_load_cb_t)(void *data, size_t data_len, void **p_priv_data);

/** Private data callbacks */
typedef struct esp_schedule_priv_data_callbacks {
    esp_schedule_priv_data_save_cb_t on_save; // Callback for saving private data to NVS.
    esp_schedule_priv_data_load_cb_t on_load; // Callback for loading private data from NVS.
} esp_schedule_priv_data_callbacks_t;
#endif

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

/** List of triggers for a schedule */
typedef struct esp_schedule_trigger_list {
    esp_schedule_trigger_t *list;
    uint8_t count;
} esp_schedule_trigger_list_t;

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
    esp_schedule_trigger_list_t triggers;
    /** Trigger callback */
    esp_schedule_trigger_cb_t trigger_cb;
    /** Timestamp callback */
    esp_schedule_timestamp_cb_t timestamp_cb;
    /** Private data associated with the schedule. This will be passed to callbacks. */
    void *priv_data;
    /** Validity of schedules. */
    esp_schedule_validity_t validity;
} esp_schedule_config_t;

/** Initialize ESP Schedule (Default - Non-NVS)
 *
 * This initializes ESP Schedule without NVS support. This must be called first before calling any of the other APIs.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_init_default(void);

#if defined(CONFIG_ESP_SCHEDULE_ENABLE_NVS) && CONFIG_ESP_SCHEDULE_ENABLE_NVS
/** Initialize ESP Schedule (NVS-enabled)
 *
 * This initializes ESP Schedule with NVS support. This must be called first before calling any of the other APIs.
 * This API also gets all the schedules from NVS.
 *
 * Note: After calling this API, the pointers to the callbacks should be updated for all the schedules by calling
 * esp_schedule_get() followed by esp_schedule_edit() with the correct callbacks.
 *
 * @param[in] nvs_partition (Optional) The NVS partition to be used. If NULL is passed, the default partition is used.
 * @param[in] priv_data_callbacks (Optional) Private data callbacks. If NULL is passed, then no private data will be saved or loaded.
 * - on_save: Callback for saving private data to NVS.
 * - on_load: Callback for loading private data from NVS.
 * @param[out] schedule_count Number of active schedules found in NVS.
 * @param[out] handles_out Array of schedule handles if any schedules have been found.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_init_nvs(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks, uint8_t *schedule_count, esp_schedule_handle_t **handles_out);
#endif

/** Initialize ESP Schedule (Legacy - Deprecated)
 *
 * @deprecated Use esp_schedule_init_default() or esp_schedule_init_nvs() instead.
 * This function is kept for backward compatibility.
 *
 * This initializes ESP Schedule. This must be called first before calling any of the other APIs.
 * This API also gets all the schedules from NVS (if it has been enabled).
 *
 * Note: After calling this API, the pointers to the callbacks should be updated for all the schedules by calling
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
 * @param[out] handle_out Pointer to store the created schedule handle.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_create(const esp_schedule_config_t *schedule_config, esp_schedule_handle_t *handle_out);

/** Remove Schedule
 *
 * This API can be used to remove an existing schedule.
 *
 * @param[in] handle Schedule handle for the schedule to be removed.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_delete(esp_schedule_handle_t handle);

/** Delete All Schedules

 * This API can be used to delete all schedules.
 * @note This API will indiscriminately erase the NVS namespace used to store schedule data, regardless of whether all schedule handles have been deleted or not.
 *
 * @param[in] handle_list List of schedule handles to be deleted.
 * @param[in] schedule_count Number of schedules to be deleted.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_delete_all(esp_schedule_handle_t *handle_list, uint8_t schedule_count);

#if defined(CONFIG_ESP_SCHEDULE_ENABLE_NVS) && CONFIG_ESP_SCHEDULE_ENABLE_NVS
/** Unload Schedule
 *
 * This API can be used to unload an existing schedule from memory.
 * The schedule will remain in NVS and can be reloaded on next initialization.
 *
 * @param[in] handle Schedule handle for the schedule to be unloaded.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_unload(esp_schedule_handle_t handle);

/** Unload All Schedules

 * This API can be used to unload all schedules from memory.
 * The schedules will remain in NVS and can be reloaded on next initialization.
 *
 * @param[in] handle_list List of schedule handles to be unloaded.
 * @param[in] schedule_count Number of schedules to be unloaded.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_unload_all(esp_schedule_handle_t *handle_list, uint8_t schedule_count);
#endif

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
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

/** Enable Schedule
 *
 * This API can be used to enable an existing schedule.
 * It can be used to enable a schedule after it has been created using esp_schedule_create()
 * or if the schedule has been disabled using esp_schedule_disable().
 *
 * @param[in] handle Schedule handle for the schedule to be enabled.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_enable(esp_schedule_handle_t handle);

/** Disable Schedule
 *
 * This API can be used to disable an on-going schedule.
 * It does not remove the schedule, just stops it. The schedule can be enabled again using
 * esp_schedule_enable().
 *
 * @param[in] handle Schedule handle for the schedule to be disabled.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_disable(esp_schedule_handle_t handle);

/** Reset Timestamps
 *
 * Trigger next times are normally cached in the trigger struct and used if valid.
 * This API can be used to reset the timestamps of all triggers of an existing schedule that are not RELATIVE.
 * This is useful when the timezone changes and the timestamps of the triggers need to be recalculated.
 *
 * @param[in] handle Schedule handle for the schedule to be reset.
 *
 * @return ESP_SCHEDULE_RET_OK on success, otherwise error code.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_reset_trigger_timestamps(esp_schedule_handle_t handle);

/** Get Schedule
 *
 * This API can be used to get details of an existing schedule.
 * The schedule_config is populated with the schedule details.
 *
 * @note The trigger list will be overwritten with a dynamically allocated array of triggers.
 * - You should free any previous trigger lists in the schedule_config (if any) before calling this API.
 * - The caller is responsible for freeing the new trigger list. Use esp_schedule_config_free_internals() for convenient cleanup.
 *
 * @param[in] handle Schedule handle.
 * @param[out] schedule_config Details of the schedule whose handle is passed.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

/** Free Internal Allocations from Schedule Config
 *
 * This helper function frees dynamically allocated memory within an esp_schedule_config_t structure.
 * This is useful for cleaning up memory allocated by esp_schedule_get().
 *
 * Currently frees:
 * - Trigger list array (schedule_config->triggers.list)
 *
 * @note This function only frees internal allocations, not the config structure itself.
 *       The config structure should be freed by the caller if it was dynamically allocated.
 *       This list may be expanded in future versions if additional internal allocations are added.
 *
 * @param[in,out] schedule_config Pointer to schedule config whose internal allocations should be freed.
 */
void esp_schedule_config_free_internals(esp_schedule_config_t *schedule_config);

/** Set Trigger Callback
 *
 * This API can be used to set the trigger callback for an existing schedule. Useful when the schedule is loaded from NVS.
 *
 * @param[in] handle Schedule handle.
 * @param[in] trigger_cb Trigger callback.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_set_trigger_callback(esp_schedule_handle_t handle, esp_schedule_trigger_cb_t trigger_cb);

/** Set Timestamp Callback
 *
 * This API can be used to set the timestamp callback for an existing schedule. Useful when the schedule is loaded from NVS.
 *
 * @param[in] handle Schedule handle.
 * @param[in] timestamp_cb Timestamp callback.
 *
 * @return ESP_SCHEDULE_RET_OK on success.
 * @return error in case of failure.
 */
ESP_SCHEDULE_RETURN_TYPE esp_schedule_set_timestamp_callback(esp_schedule_handle_t handle, esp_schedule_timestamp_cb_t timestamp_cb);


#ifdef __cplusplus
}
#endif

#endif // __ESP_SCHEDULE_UNTYPED_H__