/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <assert.h>
#include "nand_ubi_eba.h"
#include "nand_ubi_media.h"
#include "nand_ubi_priv.h"

static inline uint8_t peb_state_get(const uint8_t *arr, uint32_t pnum)
{
    return (arr[pnum / 4u] >> ((pnum % 4u) * 2u)) & 0x3u;
}

static inline void peb_state_set(uint8_t *arr, uint32_t pnum, uint8_t state)
{
    uint32_t shift = (pnum % 4u) * 2u;
    arr[pnum / 4u] = (uint8_t)((arr[pnum / 4u] & ~(uint8_t)(0x3u << shift))
                               | (uint8_t)((state & 0x3u) << shift));
}

esp_err_t nand_ubi_eba_alloc(uint32_t peb_count, uint32_t leb_count,
                              nand_ubi_eba_t *out_eba)
{
    int32_t *eba        = ubi_alloc((size_t)leb_count * sizeof(int32_t));
    uint8_t *peb_state  = ubi_alloc((size_t)(peb_count + 3u) / 4u);

    if (!eba || !peb_state) {
        free(eba);
        free(peb_state);
        return ESP_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < leb_count; i++) {
        eba[i] = UBI_LEB_UNMAPPED;
    }
    memset(peb_state, 0, (size_t)(peb_count + 3u) / 4u);

    out_eba->eba        = eba;
    out_eba->peb_state  = peb_state;
    out_eba->leb_count  = leb_count;
    out_eba->peb_count  = peb_count;
    return ESP_OK;
}

void nand_ubi_eba_free(nand_ubi_eba_t *eba)
{
    free(eba->eba);
    free(eba->peb_state);
    eba->eba        = NULL;
    eba->peb_state  = NULL;
    eba->leb_count  = 0;
    eba->peb_count  = 0;
}

int32_t nand_ubi_eba_get_pnum(const nand_ubi_eba_t *eba, uint32_t lnum)
{
    assert(lnum < eba->leb_count);
    return eba->eba[lnum];
}

void nand_ubi_eba_set(nand_ubi_eba_t *eba, uint32_t lnum, int32_t pnum)
{
    assert(lnum < eba->leb_count);
    eba->eba[lnum] = pnum;
}

bool nand_ubi_eba_peb_is_free(const nand_ubi_eba_t *eba, uint32_t pnum)
{
    assert(pnum < eba->peb_count);
    return peb_state_get(eba->peb_state, pnum) == UBI_PEB_FREE;
}

void nand_ubi_eba_peb_set_free(nand_ubi_eba_t *eba, uint32_t pnum)
{
    assert(pnum < eba->peb_count);
    peb_state_set(eba->peb_state, pnum, UBI_PEB_FREE);
}

void nand_ubi_eba_peb_set_used(nand_ubi_eba_t *eba, uint32_t pnum)
{
    assert(pnum < eba->peb_count);
    peb_state_set(eba->peb_state, pnum, UBI_PEB_USED);
}

void nand_ubi_eba_peb_set_bad(nand_ubi_eba_t *eba, uint32_t pnum)
{
    assert(pnum < eba->peb_count);
    peb_state_set(eba->peb_state, pnum, UBI_PEB_BAD);
}

int32_t nand_ubi_eba_find_free_peb(const nand_ubi_eba_t *eba, uint32_t peb_count)
{
    for (uint32_t pnum = 0; pnum < peb_count; pnum++) {
        if (peb_state_get(eba->peb_state, pnum) == UBI_PEB_FREE) {
            return (int32_t)pnum;
        }
    }
    return -1;
}
