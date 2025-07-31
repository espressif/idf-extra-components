/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @brief Triggers gcov info dump.
 *        This function waits for the host to connect to target before dumping data.
 */
void esp_gcov_dump(void);
