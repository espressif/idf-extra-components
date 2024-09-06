/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *esp_custom_part_ota_handle_t;

typedef struct {
    const esp_partition_t *update_partition;
    const esp_partition_t *backup_partition;
} esp_custom_part_ota_cfg_t;


/**
 * @brief   Commence an OTA update writing to the specified partition.
 *
 * @param config    Configuration data for the OTA process
 *
 * @return
 *    - NULL    On failure
 *    - esp_custom_part_ota_handle_t handle
*/
esp_custom_part_ota_handle_t esp_custom_part_ota_begin(esp_custom_part_ota_cfg_t config);


/**
 * @brief   Write OTA update data to partition
 *
 * This function can be called multiple times as
 * data is received during the OTA operation. Data is written
 * sequentially to the partition.
 *
 * @param ctx   esp_custom_part_ota_handle_t
 * @param data  Data buffer to write
 * @param size  Size of the data buffer in bytes
 *
 * @return
 *      - ESP_OK
 *      - ESP_ERR_INVALID_ARG
 *      - lower level esp_partition error codes.
*/
esp_err_t esp_custom_part_ota_write(esp_custom_part_ota_handle_t handle, const void *data, size_t size);


/**
 * @brief   Finish OTA update. NOTE - Please note that this does not erase the actual data on the flash. For that you may need to explicitly erase the backup partition.
 *
 * @param ctx   esp_custom_part_ota_handle_t
 *
 * @return
 *      - ESP_OK
 *      - ESP_ERR_INVALID_ARG
*/
esp_err_t esp_custom_part_ota_end(esp_custom_part_ota_handle_t handle);


/**
 * @brief   Abort the OTA update process and free the handle. NOTE - Before aborting the OTA, you need to call esp_custom_part_ota_partition_restore to restore the backup if available.
 *
 * @param ctx   esp_custom_part_ota_handle_t
 *
 * @return
 *      - ESP_OK
 *      - ESP_ERR_INVALID_ARG
*/
esp_err_t esp_custom_part_ota_abort(esp_custom_part_ota_handle_t handle);


/**
 * @brief   Backup the data from update partition to the backup partition. Default backup partition is set to the passive app partition.
 *
 * @param ctx   esp_custom_part_ota_handle_t
 * @param backup_size   size of data to backup. If set to zero, whole partition will be backed up.
 *
 * @return
 *      - ESP_OK
 *      - ESP_ERR_INVALID_ARG
 *      - ESP_FAIL
 *      - ESP_ERR_NO_MEM
 *      - lower level esp_partition error codes
*/
esp_err_t esp_custom_part_ota_partition_backup(esp_custom_part_ota_handle_t handle, size_t backup_size);


/**
 * @brief   Restore the data from the backup partition
 *
 * @param ctx esp_custom_part_ota_handle_t
 *
 * @return
 *      - ESP_OK
 *      - ESP_ERR_INVALID_ARG
 *      - ESP_ERR_NO_MEM
 *      - lower level esp_partition error codes
*/
esp_err_t esp_custom_part_ota_partition_restore(esp_custom_part_ota_handle_t handle);

#ifdef __cplusplus
}
#endif
