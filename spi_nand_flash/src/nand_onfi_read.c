/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "spi_nand_oper.h"
#include "nand_impl.h"
#include "nand_onfi_param_page.h"
#include "nand_onfi_crc.h"

static const char *TAG = "nand_onfi_read";

#define REG_CONFIG_OTP_EN   (1 << 6)

#define PARAM_PAGE_ROW_ADDR_1  0x000000
#define PARAM_PAGE_ROW_ADDR_2  0x000001
#define PARAM_PAGE_ROW_ADDR_3  0x000004

static const uint32_t param_page_row_addrs[] = {
    PARAM_PAGE_ROW_ADDR_1,
    PARAM_PAGE_ROW_ADDR_2,
    PARAM_PAGE_ROW_ADDR_3,
};

static const uint8_t onfi_signature[NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN] = { 'O', 'N', 'F', 'I' };

#define PARAM_PAGE_NUM_ADDRS  (sizeof(param_page_row_addrs) / sizeof(param_page_row_addrs[0]))

static bool is_onfi_signature_valid(const uint8_t *page_data)
{
    return memcmp(page_data, onfi_signature, NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN) == 0;
}

esp_err_t nand_onfi_read_parameter_page(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t length)
{
    esp_err_t ret;
    uint8_t orig_config = 0;

    if (handle == NULL || data == NULL || length < NAND_ONFI_PARAM_PAGE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = spi_nand_read_register(handle, REG_CONFIG, &orig_config);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t new_config = (uint8_t)(orig_config | REG_CONFIG_OTP_EN);
    ret = spi_nand_write_register(handle, REG_CONFIG, new_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ESP_ERR_NOT_FOUND;

    for (size_t addr = 0; addr < PARAM_PAGE_NUM_ADDRS; addr++) {
        spi_nand_transaction_t t = {
            .command = CMD_PAGE_READ,
            .address_bytes = 3,
            .address = param_page_row_addrs[addr],
        };
        if (spi_nand_execute_transaction(handle, &t) != ESP_OK) {
            continue;
        }

        if (nand_wait_for_ready(handle, NAND_ONFI_PARAM_PAGE_READ_WAIT_US, NULL) != ESP_OK) {
            continue;
        }

        for (int copy = 0; copy < NAND_ONFI_PARAM_PAGE_COPIES; copy++) {
            uint16_t column = (uint16_t)(copy * NAND_ONFI_PARAM_PAGE_SIZE);

            if (spi_nand_read_sio(handle, data, column, NAND_ONFI_PARAM_PAGE_SIZE) != ESP_OK) {
                continue;
            }
            if (!is_onfi_signature_valid(data)) {
                ESP_LOGD(TAG, "row addr 0x%06x copy %d: ONFI signature not found"
                         " (first bytes: %02x %02x %02x %02x)",
                         (unsigned)param_page_row_addrs[addr], copy,
                         data[0], data[1], data[2], data[3]);
                continue;
            }
            if (!nand_onfi_param_page_crc_valid(data, NAND_ONFI_PARAM_PAGE_SIZE)) {
                ESP_LOGD(TAG, "row addr 0x%06x copy %d: ONFI signature OK but CRC failed",
                         (unsigned)param_page_row_addrs[addr], copy);
                continue;
            }

            ret = ESP_OK;
            goto restore;
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "no valid ONFI parameter page found across %u row addr(s) x %d copies",
                 (unsigned)PARAM_PAGE_NUM_ADDRS, NAND_ONFI_PARAM_PAGE_COPIES);
    }

restore: {
        esp_err_t restore_ret = spi_nand_write_register(handle, REG_CONFIG, orig_config);
        if (restore_ret != ESP_OK) {
            /* Distinct from SPI errors before OTP_EN was set; caller must not fall through. */
            ESP_LOGE(TAG, "Failed to restore config register (%s); OTP access may still be enabled",
                     esp_err_to_name(restore_ret));
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ret;
}
