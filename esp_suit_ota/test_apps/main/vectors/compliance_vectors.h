/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_suit_ota_err.h"

/* one envelope from compliance.bin, see gen_vectors.py */
typedef struct {
    const char *name;
    const char *rule;          /* the draft section or RFC the vector tests */
    uint32_t offset;
    uint32_t len;
    esp_err_t expect;          /* ESP_SUIT_OTA_READY: completes and commits */
    uint32_t flags;
} suit_cvec_t;

#define CVEC_TRANSPORT  (1u << 0)   /* components are fetched through the test transport */
#define CVEC_MANUAL     (1u << 1)   /* used by a dedicated test, not by the table runner */

/* a payload the test transport serves */
typedef struct {
    const char *uri;
    uint32_t offset;
    uint32_t len;
} suit_cpayload_t;

extern const suit_cvec_t suit_cvecs[];
extern const size_t suit_cvecs_count;
extern const suit_cpayload_t suit_cpayloads[];
extern const size_t suit_cpayloads_count;
extern const uint8_t suit_cvec_device_id[16];
extern const uint32_t suit_cvec_pen;
