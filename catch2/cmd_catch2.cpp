/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: BSL-1.0
 * Note: same license as Catch2
 */
#include "esp_err.h"
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
    esp_console_cmd_t cmd = {};
    cmd.command = cmd_name,
    cmd.help = "Run tests";
    cmd.func = &cmd_catch2;
    return esp_console_cmd_register(&cmd);
}

#else // WITH_CONSOLE
// Defined to avoid ranlib warning on macOS
// (the table of contents is empty (no object file members in the library define global symbols))
extern "C" esp_err_t register_catch2(const char *cmd_name)
{
    return ESP_OK;
}
#endif // WITH_CONSOLE
