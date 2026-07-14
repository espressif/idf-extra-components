/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_blockdev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nand_ubi_eba.h"

#if CONFIG_SPIRAM
#include "esp_heap_caps.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* free() works on both paths: IDF's allocator handles all heap regions. */
static inline void *ubi_alloc(size_t size)
{
#if CONFIG_SPIRAM
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
        return p;
    }
#endif
    return malloc(size);
}

/**
 * @brief Device-level state: one instance per attached physical NAND chip.
 *
 * Not exposed in the public header; volume BDL handles (Task 5) hold a pointer
 * to this struct instead of duplicating its fields.
 */
struct nand_ubi_device {
    esp_blockdev_handle_t nand_bdl;   /**< Raw flash BDL passed to nand_ubi_attach(); not owned. */

    uint32_t peb_count;
    uint32_t peb_size;
    uint32_t page_size;
    uint32_t vid_hdr_offset;
    uint32_t data_offset;
    uint32_t leb_size;                 /**< = peb_size - data_offset. */
    uint32_t leb_count;                /**< = max_lnum + 1, known only after the attach scan; <= peb_count. */

    uint32_t image_seq;
    uint64_t global_sqnum;             /**< Highest VID sqnum observed at attach; next write uses +1. */

    nand_ubi_eba_t eba;                /**< eba[] is sized to peb_count, not leb_count: leb_count is
                                             only known once the scan finishes, but leb_count <= peb_count
                                             always holds, so peb_count is a safe upfront allocation bound. */

    SemaphoreHandle_t lock;            /**< Serializes read/write/erase/open_volume (Task 5+). */
};

#ifdef __cplusplus
}
#endif
