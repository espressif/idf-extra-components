/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_schedule_default.c
 * @brief esp_schedule_init() - the default, ESP-IDF-backed entry point.
 *
 * @warning This file must contain esp_schedule_init() and nothing else, and it
 *          must be the only file IN THIS COMPONENT that names the
 *          esp_schedule_esp_*_ops tables. Application code may name them
 *          freely - esp_schedule_esp_port.h is public so a port can be
 *          assembled from the defaults - and pays for exactly what it names.
 *          The rule here is what stops the component charging everyone for
 *          them.
 *
 *          That is what makes the port swappable at zero cost. A program that
 *          calls only esp_schedule_init_with_config() never references
 *          esp_schedule_init(), so the linker never pulls this object out of
 *          libesp_schedule.a, so it never pulls the port/esp objects either -
 *          and the FreeRTOS timer, nvs_flash, esp_sntp and esp_log
 *          dependencies come with them. This works at archive-member
 *          granularity and therefore
 *          does not depend on -ffunction-sections or --gc-sections.
 *
 *          Merging this function into esp_schedule.c, or referencing any
 *          esp_schedule_esp_*_ops from another file, silently undoes all of
 *          that: the tables become reachable from an object every caller needs,
 *          and the default port is linked into every build. check_port_isolation.py
 *          is the regression test for both mistakes; it runs from pre-commit.
 */

#include "esp_schedule.h"
#include "esp_schedule_esp_port.h"

esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count)
{
    const esp_schedule_port_config_t port = {
        .timer = esp_schedule_esp_timer_ops,
        .nvs = esp_schedule_esp_nvs_ops,
        .time_sync = esp_schedule_esp_time_sync_ops,
        .mem = esp_schedule_esp_mem_ops,
        .log = esp_schedule_esp_log,
    };
    return esp_schedule_init_with_config(&port, enable_nvs, nvs_partition, schedule_count);
}
