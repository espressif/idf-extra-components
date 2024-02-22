/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/cdefs.h>
#include "esp_types.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "sccb_io_interface.h"
#include "esp_sccb.h"

static const char *TAG = "SCCB";

esp_err_t esp_sccb_transmit(sccb_io_handle_t io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    ESP_RETURN_ON_FALSE(io_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(io_handle->transmit, ESP_ERR_NOT_SUPPORTED, TAG, "controller driver function not supported");

    return io_handle->transmit(io_handle, write_buffer, write_size, xfer_timeout_ms);
}

esp_err_t esp_sccb_receive(sccb_io_handle_t io_handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    ESP_RETURN_ON_FALSE(io_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(io_handle->receive, ESP_ERR_NOT_SUPPORTED, TAG, "controller driver function not supported");

    return io_handle->receive(io_handle, read_buffer, read_size, xfer_timeout_ms);
}

esp_err_t esp_sccb_transmit_receive(sccb_io_handle_t io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    ESP_RETURN_ON_FALSE(io_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(io_handle->transmit_receive, ESP_ERR_NOT_SUPPORTED, TAG, "controller driver function not supported");

    return io_handle->transmit_receive(io_handle, write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

esp_err_t esp_sccb_del_ctlr(sccb_io_handle_t io_handle)
{
    ESP_RETURN_ON_FALSE(io_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(io_handle->del, ESP_ERR_NOT_SUPPORTED, TAG, "controller driver function not supported");

    return io_handle->del(io_handle);
}
