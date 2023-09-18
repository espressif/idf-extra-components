/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <catch2/catch_session.hpp>

extern "C" void app_main(void)
{
    int argc = 1;
    const char *argv[2] = {
        "target_test_main",
        NULL
    };

    auto result = Catch::Session().run(argc, argv);
    if (result != 0) {
        printf("Test failed with result %d\n", result);
    } else {
        printf("Test passed.\n");
    }
}
