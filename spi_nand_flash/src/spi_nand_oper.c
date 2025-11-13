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
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
#include "esp_private/esp_cache_private.h"
#endif

esp_err_t spi_nand_execute_transaction(spi_nand_flash_device_t *handle, spi_nand_transaction_t *transaction)
{
    uint8_t half_duplex = handle->config.flags & SPI_DEVICE_HALFDUPLEX;
    if (!half_duplex) {
        uint32_t len = transaction->miso_len > transaction->mosi_len ? transaction->miso_len : transaction->mosi_len;
        transaction->miso_len = len;
        transaction->mosi_len = len;
    }

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

    if (transaction->flags & SPI_TRANS_USE_TXDATA) {
        assert(transaction->mosi_len <= 4 && "SPI_TRANS_USE_TXDATA used for a long transaction");
        memcpy(e.base.tx_data, transaction->mosi_data, transaction->mosi_len);
    }
    if (transaction->flags & SPI_TRANS_USE_RXDATA) {
        assert(transaction->miso_len <= 4 && "SPI_TRANS_USE_RXDATA used for a long transaction");
    }

    esp_err_t ret = spi_device_transmit(handle->config.device_handle, (spi_transaction_t *) &e);
    if (ret == ESP_OK) {
        if (transaction->flags == SPI_TRANS_USE_RXDATA) {
            memcpy(transaction->miso_data, e.base.rx_data, transaction->miso_len);
        }
    }
    return ret;
}

esp_err_t spi_nand_read_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t *val)
{
    spi_nand_transaction_t t = {
        .command = CMD_READ_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .miso_len = 1,
        .miso_data = val,
        .flags = SPI_TRANS_USE_RXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t val)
{
    spi_nand_transaction_t t = {
        .command = CMD_SET_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .mosi_len = 1,
        .mosi_data = &val,
        .flags = SPI_TRANS_USE_TXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_enable(spi_nand_flash_device_t *handle)
{
    spi_nand_transaction_t t = {
        .command = CMD_WRITE_ENABLE
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_read_page(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PAGE_READ,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
static uint16_t check_length_alignment(spi_nand_flash_device_t *handle, uint16_t length)
{
    size_t alignment;
    uint16_t data_len = length;
    esp_cache_get_alignment(MALLOC_CAP_DMA, &alignment);
    bool is_length_unaligned = (length & (alignment - 1)) ? true : false;
    if (is_length_unaligned) {
        if (length < alignment) {
            data_len = ((length + alignment) & ~(alignment - 1));
        } else {
            data_len = ((length + (alignment - 1)) & ~(alignment - 1));
        }
    }
    if (!(handle->config.flags & SPI_DEVICE_HALFDUPLEX)) {
        data_len = data_len + alignment;
    }
    return data_len;
}
#endif

static esp_err_t spi_nand_quad_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_TRANS_MODE_QIO;
    uint8_t cmd = CMD_READ_X4;
    uint8_t dummy_bits = 8;

    uint8_t *data_read = data;
    uint16_t data_read_len = length;
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    data_read = handle->temp_buffer;
    data_read_len = check_length_alignment(handle, length);
#endif

    if (handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        spi_flags |= SPI_TRANS_MULTILINE_ADDR;
        cmd = CMD_READ_QIO;
        dummy_bits = 4;
    }

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
        .dummy_bits = dummy_bits,
        .flags = spi_flags,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret =  spi_nand_execute_transaction(handle, &t);

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    memcpy(data, data_read, length);
#endif
    return ret;
}

static esp_err_t spi_nand_dual_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_TRANS_MODE_DIO;
    uint8_t cmd = CMD_READ_X2;
    uint8_t dummy_bits = 8;

    uint8_t *data_read = data;
    uint16_t data_read_len = length;
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    data_read = handle->temp_buffer;
    data_read_len = check_length_alignment(handle, length);
#endif

    if (handle->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        spi_flags |= SPI_TRANS_MULTILINE_ADDR;
        cmd = CMD_READ_DIO;
        dummy_bits = 4;
    }

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
        .dummy_bits = dummy_bits,
        .flags = spi_flags,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret =  spi_nand_execute_transaction(handle, &t);

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    memcpy(data, data_read, length);
#endif
    return ret;
}

static esp_err_t spi_nand_fast_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint8_t *data_read = data;
    uint16_t data_read_len = length;
    uint8_t half_duplex = handle->config.flags & SPI_DEVICE_HALFDUPLEX;
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    data_read = handle->temp_buffer;
    data_read_len = check_length_alignment(handle, length);
#else
    if (!half_duplex) {
        data_read_len = length + 1;
        data_read = handle->temp_buffer;
    }
#endif

    spi_nand_transaction_t t = {
        .command = CMD_READ_FAST,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        .flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL,
#endif
    };

    if (half_duplex) {
        t.dummy_bits = 8;
    }
    esp_err_t ret = spi_nand_execute_transaction(handle, &t);
    if (ret != ESP_OK) {
        goto fail;
    }

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    if (!half_duplex) {
        memcpy(data, data_read + 1, length);
    } else {
        memcpy(data, data_read, length);
    }
#else
    if (!half_duplex) {
        memcpy(data, data_read + 1, length);
    }
#endif

fail:
    return ret;
}

esp_err_t spi_nand_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    if (handle->config.io_mode == SPI_NAND_IO_MODE_DOUT || handle->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        return spi_nand_dual_read(handle, data, column, length);
    } else if (handle->config.io_mode == SPI_NAND_IO_MODE_QOUT || handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        return spi_nand_quad_read(handle, data, column, length);
    }
    return spi_nand_fast_read(handle, data, column, length);
}

esp_err_t spi_nand_program_execute(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PROGRAM_EXECUTE,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_program_load(spi_nand_flash_device_t *handle, const uint8_t *data, uint16_t column, uint16_t length)
{
    uint8_t cmd = CMD_PROGRAM_LOAD;
    uint32_t spi_flags = 0;
    if (handle->config.io_mode == SPI_NAND_IO_MODE_QOUT || handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        cmd = CMD_PROGRAM_LOAD_X4;
        spi_flags = SPI_TRANS_MODE_QIO;
    }

    uint16_t data_write_len = length;
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    memcpy(handle->temp_buffer, data, length);
    const uint8_t *data_write = handle->temp_buffer;
    data_write_len = check_length_alignment(handle, length);
#endif

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .mosi_len = data_write_len,
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
        .mosi_data = data_write,
#else
        .mosi_data = data,
#endif
        .flags = spi_flags,
    };
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_erase_block(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t  t = {
        .command = CMD_ERASE_BLOCK,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}
