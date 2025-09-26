## Overview

When a task stack resides in PSRAM, invoking `esp_flash_*` directly in that task can cause crashes during moments when the Flash driver temporarily disables the CPU cache (resulting in the PSRAM stack being inaccessible). Typical failures include Cache errors and Guru Meditation.

The `esp_flash_dispatcher` component intercepts common Flash APIs and executes the real Flash operations in a dedicated background task whose stack lives in internal RAM. This guarantees that even while cache is disabled, Flash operations run on an always-accessible stack. Therefore, application tasks can keep their stacks in PSRAM without worrying about Flash operations breaking them.

## Usage

1. Add esp_flash_dispatcher component to your project: `idf.py add-dependency espressif/esp_flash_dispatcher`
2. Initialize esp_flash_dispatcher in app_main:

    ```c
    #include "esp_flash_dispatcher.h"
    
    const esp_flash_dispatcher_config_t cfg = {
        .task_stack_size = 2048,
        .task_priority = 10,
        .task_core_id = tskNO_AFFINITY,
        .queue_size = 5,
    };
    ESP_ERROR_CHECK(esp_flash_dispatcher_init(&cfg));
    ```
    
3. Now you can call any API which writes or reads SPI Flash from a task with the stack in PSRAM, no other changes are required.
