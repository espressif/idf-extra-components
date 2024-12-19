/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "esp_https_server.h"

/**
 * @brief starts the https server
 *
 * @param bin_size the size of binary image which will be exposed
 *                 by the server. NOTE - bin_size connot be 0.
 */
esp_err_t example_test_start_webserver(void);

/**
 * @brief Takes the firmware URL from the STDIN (if want to send
 *         other data write the data in just one line by adding " " deleminator).
 *
 * @param data pointer to the firmware URL (or URL including other data)
 */
void example_test_firmware_data_from_stdin(const char **data);
