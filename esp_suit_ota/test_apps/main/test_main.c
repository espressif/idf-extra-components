/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "nvs_flash.h"
#include "esp_event.h"

void app_main(void)
{
    nvs_flash_erase();
    nvs_flash_init();
    esp_event_loop_create_default();
    printf("esp_suit_ota tests\n");
    unity_run_menu();
}
