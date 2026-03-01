/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_nvs.h
 * @brief NVS interface for the esp_schedule component
 */

#ifndef __GLUE_NVS_H__
#define __GLUE_NVS_H__

/* Includes **********************************************************************/
#include <stddef.h>
#include <stdint.h>

/* Types **********************************************************************/

/**
 * @brief Handle to the NVS partition.
 */
typedef void *esp_schedule_nvs_handle_t;

/**
 * @brief Iterator to the NVS partition.
 */
typedef void *esp_schedule_nvs_iterator_t;

/**
 * @brief Open mode for the NVS partition.
 */
typedef enum {
    ESP_SCHEDULE_NVS_OPEN_READONLY = 0,
    ESP_SCHEDULE_NVS_OPEN_READWRITE = 1,
} esp_schedule_nvs_open_mode_t;

typedef enum {
    ESP_SCHEDULE_NVS_ERROR = -1,
    ESP_SCHEDULE_NVS_OK = 0,
    ESP_SCHEDULE_NVS_NOT_FOUND = 1,
    ESP_SCHEDULE_NVS_NO_MEM = 2,
} esp_schedule_nvs_error_t;

/* Functions ******************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open the NVS partition from the given partition label and name space.
 *
 * @param[in] partition_label The label of the partition to open.
 * @param[in] name_space The name space to open.
 * @param[in] mode The open mode to use.
 * @param[out] p_handle The handle to the NVS partition.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_open_from_partition(const char *partition_label, const char *name_space, esp_schedule_nvs_open_mode_t mode, esp_schedule_nvs_handle_t *p_handle);

/**
 * @brief Close the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 */
void esp_schedule_nvs_close(esp_schedule_nvs_handle_t handle);

/**
 * @brief Commit the changes to the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_commit(esp_schedule_nvs_handle_t handle);


/**
 * @brief Erase a key from the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @param[in] key The key to erase.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_erase_key(esp_schedule_nvs_handle_t handle, const char *key);

/**
 * @brief Erase all keys from the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_erase_all(esp_schedule_nvs_handle_t handle);

/**
 * @brief Set a blob in the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @param[in] key The key to set.
 * @param[in] value The value to set.
 * @param[in] value_len The length of the value.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_set_blob(esp_schedule_nvs_handle_t handle, const char *key, const void *value, size_t value_len);

/**
 * @brief Get a blob from the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @param[in] key The key to get.
 * @param[out] value The value to get. If value is NULL, the length of the value will be returned in p_value_len.
 * @param[out] p_value_len The length of the value.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_get_blob(esp_schedule_nvs_handle_t handle, const char *key, void *value, size_t *p_value_len);

/**
 * @brief Set a u8 in the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @param[in] key The key to set.
 * @param[in] value The value to set.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_set_u8(esp_schedule_nvs_handle_t handle, const char *key, uint8_t value);

/**
 * @brief Get a u8 from the NVS partition.
 *
 * @param[in] handle The handle to the NVS partition.
 * @param[in] key The key to get.
 * @param[out] value The value to get.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_get_u8(esp_schedule_nvs_handle_t handle, const char *key, uint8_t *value);

/**
 * @brief Find the blobs in the NVS partition.
 *
 * @param[in] partition_label The label of the partition to find the blobs in.
 * @param[in] name_space The name space to find the blobs in.
 * @param[out] iterator The iterator to the NVS partition.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_entry_find_blobs(const char *partition_label, const char *name_space, esp_schedule_nvs_iterator_t *iterator);

/**
 * @brief Get the information about the next entry in the NVS partition.
 *
 * @param[in] iterator The iterator to the NVS partition.
 * @param[out] p_key Pointer at which to store the key of the next entry. *p_key needs to be freed by the caller.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code. If the key is not found, the pointer will be NULL.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_entry_get_key(esp_schedule_nvs_iterator_t iterator, char **p_key);

/**
 * @brief Move the iterator to the next entry in the NVS partition.
 *
 * @param[in] iterator The iterator to the NVS partition.
 * @return ESP_SCHEDULE_NVS_OK on success, otherwise error code.
 */
esp_schedule_nvs_error_t esp_schedule_nvs_entry_next(esp_schedule_nvs_iterator_t *iterator);

/**
 * @brief Release the iterator.
 *
 * @param[in] iterator The iterator to the NVS partition.
 */
void esp_schedule_nvs_release_iterator(esp_schedule_nvs_iterator_t iterator);

#ifdef __cplusplus
}
#endif

#endif // __GLUE_NVS_H__