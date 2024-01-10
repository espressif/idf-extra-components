/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Organization of the RGB QEMU panel registers */
typedef volatile struct rgb_qemu_dev_s {
    union {
        struct {
            uint32_t minor: 16;
            uint32_t major: 16;
        };
        uint32_t val;
    } version;
    union {
        struct {
            uint32_t height: 16;
            uint32_t width: 16;
        };
        uint32_t val;
    } size;
    union {
        struct {
            uint32_t y: 16;
            uint32_t x: 16;
        };
        uint32_t val;
    } update_from;
    union {
        struct {
            uint32_t y: 16;
            uint32_t x: 16;
        };
        uint32_t val;
    } update_to;
    /* Address of the buffer containing the new pixels of the area defined above */
    void *update_content;
    union {
        struct {
            uint32_t ena: 1;
            uint32_t reserved: 31;
        };
        uint32_t val;
    } update_st;
    uint32_t bpp;
} rgb_qemu_dev_t;

#ifdef __cplusplus
}
#endif
