/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UBI_PEB_FREE            0u
#define UBI_PEB_USED            1u
#define UBI_PEB_BAD             2u
#define UBI_PEB_ERASE_PENDING   3u

typedef struct {
    int32_t  *eba;          /* [leb_count]: lnum → pnum; UBI_LEB_UNMAPPED if unmapped */
    uint8_t  *peb_state;    /* (peb_count + 3) / 4 bytes; 2 bits per PEB (UBI_PEB_*) */
    uint32_t  leb_count;
    uint32_t  peb_count;
} nand_ubi_eba_t;

esp_err_t nand_ubi_eba_alloc(uint32_t peb_count, uint32_t leb_count,
                              nand_ubi_eba_t *out_eba);
void      nand_ubi_eba_free(nand_ubi_eba_t *eba);

int32_t   nand_ubi_eba_get_pnum(const nand_ubi_eba_t *eba, uint32_t lnum);
void      nand_ubi_eba_set(nand_ubi_eba_t *eba, uint32_t lnum, int32_t pnum);

bool      nand_ubi_eba_peb_is_free(const nand_ubi_eba_t *eba, uint32_t pnum);
void      nand_ubi_eba_peb_set_free(nand_ubi_eba_t *eba, uint32_t pnum);
void      nand_ubi_eba_peb_set_used(nand_ubi_eba_t *eba, uint32_t pnum);
void      nand_ubi_eba_peb_set_bad(nand_ubi_eba_t *eba, uint32_t pnum);
void      nand_ubi_eba_peb_set_erase_pending(nand_ubi_eba_t *eba, uint32_t pnum);

int32_t   nand_ubi_eba_find_free_peb(const nand_ubi_eba_t *eba, uint32_t peb_count);

#ifdef __cplusplus
}
#endif
