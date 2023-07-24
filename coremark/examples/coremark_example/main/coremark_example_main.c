/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"

// In IDF v5.x, there is a common CPU frequency option for all targets
#if defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
#define CPU_FREQ CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ

// In IDF v4.x, CPU frequency options were target-specific
#elif defined(CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ)
#define CPU_FREQ CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ
#elif defined(CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ)
#define CPU_FREQ CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ
#elif defined(CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ)
#define CPU_FREQ CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ
#elif defined(CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ)
#define CPU_FREQ CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ
#endif

// Entry point of coremark benchmark
extern int main(void);

void app_main(void)
{
    printf("Running coremark...\n");
    main();
    printf("CPU frequency: %d MHz\n", CPU_FREQ);
}
