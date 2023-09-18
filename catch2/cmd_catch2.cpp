/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: BSL-1.0
 * Note: same license as Catch2
 */
#if WITH_CONSOLE
#include "Catch2/src/catch2/catch_config.hpp"
#include "esp_console.h"
#include "catch2/catch_session.hpp"
#include "cmd_catch2.h"

static int cmd_catch2(int argc, char **argv)
{
    static auto session = Catch::Session();
    Catch::ConfigData configData;
    session.useConfigData(configData);
    return session.run(argc, argv);
}

extern "C" esp_err_t register_catch2(const char *cmd_name)
{
    const esp_console_cmd_t cmd = {
        .command = cmd_name,
        .help = "Run tests",
        .hint = NULL,
        .func = &cmd_catch2,
        .argtable = NULL
    };
    return esp_console_cmd_register(&cmd);
}

#endif // WITH_CONSOLE
