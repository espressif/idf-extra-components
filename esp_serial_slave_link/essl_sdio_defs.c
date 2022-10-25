/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Definitions of Espressif SDIO Slave hardware


#include "essl_sdio.h"

static uint16_t get_reg_addr(int pos);

essl_sdio_def_t ESSL_SDIO_DEF_ESP32 = {
    .new_packet_intr_mask = BIT(23),
    .token_rdata_reg = 0x44,
    .int_raw_reg = 0x50,
    .int_st_reg = 0x58,
    .pkt_len_reg = 0x60,
    .slave_intr_reg = 0x8c,
    .int_clr_reg = 0xd4,
    .func1_int_ena_reg = 0xdc,
    .get_reg_addr = get_reg_addr,
};

static uint16_t get_reg_addr(int pos)
{
    const int W0_addr = 0x6c;
    if (pos > 31) {
        return W0_addr + 16;
    } else if (pos > 23) {
        return W0_addr + 4;
    } else {
        return W0_addr;
    }
}
