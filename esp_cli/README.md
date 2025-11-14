# esp_cli Component

The `esp_cli` component provides a **Runtime Evaluation Loop (REPL)** mechanism for ESP-IDF-based applications.  
It allows developers to build interactive command-line interfaces (CLI) that support user-defined commands, history management, and customizable callbacks for command execution.

This component integrates with [`esp_linenoise`](../esp_linenoise) for line editing and input handling, and with [`esp_cli_commands`](../esp_cli_commands) for command parsing and execution.

---

## Features

- Modular REPL management with explicit `start` and `stop` control
- Integration with [`esp_linenoise`](../esp_linenoise) for input and history
- Support for command sets through [`esp_cli_commands`](../esp_cli_commands)
- Configurable callbacks for:
  - Pre-execution processing
  - Post-execution handling
  - On-stop and on-exit events
- Thread-safe operation using FreeRTOS semaphores
- Optional command history persistence to filesystem

---

## Usage

A typical use case involves:

1. Initializing `esp_linenoise` and `esp_cli_commands`
2. Creating the esp_cli instance with `esp_cli_create()`
3. Running `esp_cli()` in a task
4. Starting and stopping the esp_cli using `esp_cli_start()` and `esp_cli_stop()`
5. Destroying the instance with `esp_cli_destroy()` when done

### Example

```c
#include "esp_cli.h"
#include "esp_linenoise.h"
#include "esp_cli_commands.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "repl_example";

void cli_task(void *arg)
{
    esp_cli_handle_t repl_hdl = (esp_cli_handle_t)arg;

    // Run REPL loop (blocking until esp_cli_stop() is called)
    // The loop won't be reached until esp_cli_start() is called
    esp_cli(repl_hdl);

    ESP_LOGI(TAG, "esp_cli instance task exiting");
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;
    esp_cli_handle_t cli = NULL;

    // Initialize esp_linenoise (mandatory)
    esp_linenoise_handle_t esp_linenoise_hdl = esp_linenoise_create();

    // Initialize command set (optional)
    esp_cli_command_set_handle_t esp_cli_commands_cmd_set = esp_cli_commands_create();

    esp_cli_config_t cli_cfg = {
        .linenoise_handle = esp_linenoise_hdl,
        .command_set_handle = esp_cli_commands_cmd_set, /* optional */
        .max_cmd_line_size = 256,
        .history_save_path = "/spiffs/cli_history.txt", /* optional */
    };

    ret = esp_cli_create(&cli_cfg, &cli);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create esp_cli instance (%s)", esp_err_to_name(ret));
        return;
    }

    // Create esp_cli instance task
    if (xTaskCreate(cli_task, "cli_task", 4096, cli, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create esp_cli instance task");
        esp_cli_destroy(cli);
        return;
    }

    ESP_LOGI(TAG, "Starting esp_cli...");
    ret = esp_cli_start(cli);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start esp_cli (%s)", esp_err_to_name(ret));
        esp_cli_destroy(cli);
        return;
    }

    // Application logic can run in parallel while esp_cli instance runs in its own task
    // [...]
    vTaskDelay(pdMS_TO_TICKS(10000)); // Example delay

    // Stop esp_cli instance
    ret = esp_cli_stop(cli);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop esp_cli (%s)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "esp_cli exited");

    // Destroy esp_cli instance and clean up
    ret = esp_cli_destroy(cli);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to destroy esp_cli instance cleanly (%s)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "esp_cli example finished");
}

```
