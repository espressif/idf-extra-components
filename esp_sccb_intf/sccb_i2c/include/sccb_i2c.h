/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/i2c_master.h"
#include "sccb_types.h"

/**
 * @brief SCCB I2C configuration
 */
typedef struct {
    i2c_addr_bit_len_t dev_addr_length;   ///< Select the address length of the slave device
    uint16_t device_address;              ///< I2C device raw address. (The 7/10 bit address without read/write bit)
    uint32_t scl_speed_hz;                ///< I2C SCL line frequency
} sccb_i2c_config_t;

/**
 * @brief New I2C IO handle
 *
 * @param[in]  bus_handle
 * @param[in]  config
 * @param[out] io_handle
 *
 * @return
 *        - ESP_OK:  On success
 *        - ESP_ERR_NO_MEM: Out of memory
 */
esp_err_t sccb_new_i2c_io(i2c_master_bus_handle_t bus_handle, const sccb_i2c_config_t *config, sccb_io_handle_t *io_handle);
