/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "spi_nand_oper.h"
#include "driver/spi_master.h"

esp_err_t spi_nand_execute_transaction(spi_device_handle_t device, spi_nand_transaction_t *transaction)
{
    spi_transaction_ext_t e = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_ADDR |  SPI_TRANS_VARIABLE_CMD |  SPI_TRANS_VARIABLE_DUMMY | transaction->flags,
            .rxlength = transaction->miso_len * 8,
            .rx_buffer = transaction->miso_data,
            .length = transaction->mosi_len * 8,
            .tx_buffer = transaction->mosi_data,
            .addr = transaction->address,
            .cmd = transaction->command
        },
        .address_bits = transaction->address_bytes * 8,
        .command_bits = 8,
        .dummy_bits = transaction->dummy_bits
    };

    if (transaction->flags == SPI_TRANS_USE_TXDATA) {
        assert(transaction->mosi_len <= 4 && "SPI_TRANS_USE_TXDATA used for a long transaction");
        memcpy(e.base.tx_data, transaction->mosi_data, transaction->mosi_len);
    }
    if (transaction->flags == SPI_TRANS_USE_RXDATA) {
        assert(transaction->miso_len <= 4 && "SPI_TRANS_USE_RXDATA used for a long transaction");
    }

    esp_err_t ret = spi_device_transmit(device, (spi_transaction_t *) &e);
    if (ret == ESP_OK) {
        if (transaction->flags == SPI_TRANS_USE_RXDATA) {
            memcpy(transaction->miso_data, e.base.rx_data, transaction->miso_len);
        }
    }
    return ret;
}

esp_err_t spi_nand_read_register(spi_device_handle_t device, uint8_t reg, uint8_t *val)
{
    spi_nand_transaction_t t = {
        .command = CMD_READ_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .miso_len = 1,
        .miso_data = val,
        .flags = SPI_TRANS_USE_RXDATA,
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_write_register(spi_device_handle_t device, uint8_t reg, uint8_t val)
{
    spi_nand_transaction_t  t = {
        .command = CMD_SET_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .mosi_len = 1,
        .mosi_data = &val,
        .flags = SPI_TRANS_USE_TXDATA,
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_write_enable(spi_device_handle_t device)
{
    spi_nand_transaction_t  t = {
        .command = CMD_WRITE_ENABLE
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_read_page(spi_device_handle_t device, uint32_t page)
{
    spi_nand_transaction_t  t = {
        .command = CMD_PAGE_READ,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_read(spi_device_handle_t device, uint8_t *data, uint16_t column, uint16_t length)
{
    spi_nand_transaction_t  t = {
        .command = CMD_READ_FAST,
        .address_bytes = 2,
        .address = column,
        .miso_len = length,
        .miso_data = data,
        .dummy_bits = 8,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        .flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL,
#endif
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_program_execute(spi_device_handle_t device, uint32_t page)
{
    spi_nand_transaction_t  t = {
        .command = CMD_PROGRAM_EXECUTE,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_program_load(spi_device_handle_t device, const uint8_t *data, uint16_t column, uint16_t length)
{
    spi_nand_transaction_t  t = {
        .command = CMD_PROGRAM_LOAD,
        .address_bytes = 2,
        .address = column,
        .mosi_len = length,
        .mosi_data = data
    };

    return spi_nand_execute_transaction(device, &t);
}

esp_err_t spi_nand_erase_block(spi_device_handle_t device, uint32_t page)
{
    spi_nand_transaction_t  t = {
        .command = CMD_ERASE_BLOCK,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(device, &t);
}
