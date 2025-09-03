# esp_repl Component

The `esp_repl` component provides a **Runtime Evaluation Loop (REPL)** mechanism for ESP-IDF-based applications.  
It allows developers to build interactive command-line interfaces (CLI) that support user-defined commands, history management, and customizable callbacks for command execution.

This component integrates with [`esp_linenoise`](../esp_linenoise) for line editing and input handling, and with [`esp_commands`](../esp_commands) for command parsing and execution.

---

## Features

- Modular REPL management with explicit `start` and `stop` control
- Integration with [`esp_linenoise`](../esp_linenoise) for input and history
- Support for command sets through [`esp_commands`](../esp_commands)
- Configurable callbacks for:
  - Pre-execution processing
  - Post-execution handling
  - On-stop and on-exit events
- Thread-safe operation using FreeRTOS semaphores
- Optional command history persistence to filesystem

---

## Usage

A typical use case involves:

1. Initializing `esp_linenoise` and `esp_commands`
2. Creating the REPL instance with `esp_repl_create()`
3. Running `esp_repl()` in a task
4. Starting and stopping the REPL using `esp_repl_start()` and `esp_repl_stop()`
5. Destroying the instance with `esp_repl_destroy()` when done

### Example

```c
#include "esp_repl.h"
#include "esp_linenoise.h"
#include "esp_commands.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "repl_example";

void repl_task(void *arg)
{
    esp_repl_handle_t repl_hdl = (esp_repl_handle_t)arg;

    // Run REPL loop (blocking until esp_repl_stop() is called)
    // The loop won't be reached until esp_repl_start() is called
    esp_repl(repl_hdl);

    ESP_LOGI(TAG, "REPL task exiting");
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;
    esp_repl_handle_t repl = NULL;

    // Initialize esp_linenoise (mandatory)
    esp_linenoise_handle_t esp_linenoise_hdl = esp_linenoise_create();

    // Initialize command set (optional)
    esp_command_set_handle_t esp_commands_cmd_set = esp_commands_create();

    esp_repl_config_t repl_cfg = {
        .linenoise_handle = esp_linenoise_hdl,
        .command_set_handle = esp_commands_cmd_set, /* optional */
        .max_cmd_line_size = 256,
        .history_save_path = "/spiffs/repl_history.txt", /* optional */
    };

    ret = esp_repl_create(&repl_cfg, &repl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create REPL instance (%s)", esp_err_to_name(ret));
        return;
    }

    // Create REPL task
    if (xTaskCreate(repl_task, "repl_task", 4096, repl, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create REPL task");
        esp_repl_destroy(repl);
        return;
    }

    ESP_LOGI(TAG, "Starting REPL...");
    ret = esp_repl_start(repl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start REPL (%s)", esp_err_to_name(ret));
        esp_repl_destroy(repl);
        return;
    }

    // Application logic can run in parallel while REPL runs in its own task
    // [...]
    vTaskDelay(pdMS_TO_TICKS(10000)); // Example delay

    // Stop REPL
    ret = esp_repl_stop(repl);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop REPL (%s)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "REPL exited");

    // Destroy REPL instance and clean up
    ret = esp_repl_destroy(repl);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to destroy REPL instance cleanly (%s)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "REPL example finished");
}

```
