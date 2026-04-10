/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lz4_example.h"

void app_main(void)
{
    lz4_run_block_example();
    lz4_run_stream_example();
}
