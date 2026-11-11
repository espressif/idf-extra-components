/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include "esp_suit_ota.h"
#include "compliance_vectors.h"

/* a sink in RAM, so most tests never touch flash */
typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t size;
    int closes;
} mem_sink_t;

extern mem_sink_t g_mem[3];

/*
 * The device every compliance vector targets: the Appendix B key, vendor and class, the enterprise
 * number and device identifier of gen_vectors.py, and components [h'00'], [h'01'], [h'02'] in the
 * three memory sinks.
 */
extern const esp_suit_ota_config_t g_cfg;

#define TEST_MANIFEST_URL "https://test.local/manifest.suit"

/* NVS state of the component and the slot manager erased, sinks emptied */
void test_reset_state(void);

const suit_cvec_t *cvec_find(const char *name);
const uint8_t *cvec_data(const suit_cvec_t *v);

/*
 * One campaign: begin, feed the envelope chunk bytes at a time (a payload fetched through the test
 * transport when transport is set), signal the end of each resource, commit when ready, abort.
 * Returns the first status that is not ESP_OK; ESP_SUIT_OTA_READY means ready and committed.
 */
esp_err_t run_campaign(const esp_suit_ota_config_t *cfg, const uint8_t *env, size_t len, size_t chunk, bool transport);

/* the same, leaving the handle open after a successful commit */
esp_err_t run_campaign_keep(const esp_suit_ota_config_t *cfg, const uint8_t *env, size_t len, size_t chunk,
                            bool transport, esp_suit_ota_handle_t *out);

esp_err_t run_cvec(const suit_cvec_t *v, size_t chunk);
