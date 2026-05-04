/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#include "nand_oob_layout_default.h"

static esp_err_t default_free_region(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section == 0) {
        out->offset = 2;
        out->length = 2;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static const spi_nand_ooblayout_ops_t s_nand_oob_layout_default_ops = {
    .free_region = default_free_region,
    .ecc_region = NULL,
};

/*
 * oob_bytes == 0: spare length comes from chip geometry / emulated_page_oob at init (step 05).
 * BBM is not listed by free_region; bytes 2-3 are the sole FREE_ECC user region (PAGE_USED).
 */
static const spi_nand_oob_layout_t s_nand_oob_layout_default = {
    .oob_bytes = 0,
    .bbm =
    {
        .bbm_offset = 0,
        .bbm_length = 2,
        .good_pattern = {0xFF, 0xFF},
        .check_pages_mask = SPI_NAND_BBM_CHECK_FIRST_PAGE,
    },
    .ops = &s_nand_oob_layout_default_ops,
};

const spi_nand_oob_layout_t *nand_oob_layout_get_default(void)
{
    return &s_nand_oob_layout_default;
}
