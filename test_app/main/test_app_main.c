/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"

void app_main(void)
{
    UNITY_BEGIN();
    //unity_run_tests_by_tag("[libsodium]", false);
    unity_run_all_tests();
    //unity_run_menu();
    UNITY_END();
}
