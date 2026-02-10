/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#ifdef CONFIG_EXAMPLE_ENABLE_CI_TEST

/**
 * @brief starts the https server
 *
 * @param patch_size the size of patch file which will be exposed
 *                   by the server. NOTE - patch_size cannot be 0.
 */
esp_err_t delta_ota_test_start_webserver(void);

/**
 * @brief Takes the firmware URL from the STDIN (if want to send
 *         other data write the data in just one line by adding " " delimiter).
 *
 * @param data pointer to the firmware URL (or URL including other data)
 */
void delta_ota_test_firmware_data_from_stdin(const char **data);
#endif
