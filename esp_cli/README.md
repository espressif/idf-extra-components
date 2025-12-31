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
#include <string.h>
#include <fcntl.h>
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_cli.h"
#include "esp_cli_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXAMPLE_COMMAND_MAX_LENGTH 128

static const char *TAG = "repl_example";

static int example_cmd_func(void *context, esp_cli_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    (void)context; /* this is NULL and useless for the help command */
    (void)argc;
    (void)argv;

    const char example_cmd_msg[] = "example command output\n";
    cmd_args->write_func(cmd_args->out_fd, example_cmd_msg, strlen(example_cmd_msg));
    return 0;
}

static const char *example_cmd_hint(void *context)
{
    (void)context;
    return "example cmd hint";
}

static const char *example_cmd_glossary(void *context)
{
    (void)context;
    return "example command glossary";
}

static const char example_cmd_help_str[] = "example command help";

ESP_CLI_COMMAND_REGISTER(cmd,
                         cmd,
                         example_cmd_help_str,
                         example_cmd_func,
                         NULL,
                         example_cmd_hint,
                         example_cmd_glossary);

static void example_completion_cb(const char *str, void *cb_ctx, esp_linenoise_completion_cb_t cb)
{
    esp_cli_commands_get_completion(NULL, str, cb_ctx, cb);
}

static char *example_hints_cb(const char *str, int *color, int *bold)
{
    /* return the hint of a given command */
    return NULL;
}

static void example_free_hints_cb(void *ptr)
{
    /* free the hint pointed at by the pointer in parameter */
}

static void example_cli_task(void *arg)
{
    esp_cli_handle_t repl_hdl = (esp_cli_handle_t)arg;

    // Run REPL loop (blocking until esp_cli_stop() is called)
    // The loop won't be reached until esp_cli_start() is called
    esp_cli(repl_hdl);

    ESP_LOGI(TAG, "esp_cli instance task exiting");
    vTaskDelete(NULL);
}

static void example_init_io(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
#if SOC_UART_SUPPORT_REF_TICK
            .source_clk = UART_SCLK_REF_TICK,
#elif SOC_UART_SUPPORT_XTAL_CLK
            .source_clk = UART_SCLK_XTAL,
#endif
    };
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

    /* Tell VFS to use UART driver */
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_cdcacm_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_cdcacm_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

    usb_serial_jtag_driver_config_t jtag_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };

    /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( usb_serial_jtag_driver_install(&jtag_config));

    /* Tell vfs to use usb-serial-jtag driver */
    usb_serial_jtag_vfs_use_driver();

#else
#error Unsupported console type
#endif

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);
}

void app_main(void)
{
    esp_err_t ret;
    esp_cli_handle_t cli = NULL;

    /* configure the IO used by the esp_cli */
    example_init_io();

    // Initialize esp_linenoise (mandatory)
    esp_linenoise_handle_t esp_linenoise_hdl = NULL;
    esp_linenoise_config_t esp_linenoise_config;
    esp_linenoise_config.prompt = ">";
    esp_linenoise_config.max_cmd_line_length = EXAMPLE_COMMAND_MAX_LENGTH;
    esp_linenoise_config.history_max_length = 16;
    esp_linenoise_config.in_fd = STDIN_FILENO;
    esp_linenoise_config.out_fd = STDOUT_FILENO;
    esp_linenoise_config.allow_multi_line = true;
    esp_linenoise_config.allow_empty_line = true;
    esp_linenoise_config.allow_dumb_mode = false;
    esp_linenoise_config.completion_cb = example_completion_cb;
    esp_linenoise_config.hints_cb = example_hints_cb;
    esp_linenoise_config.free_hints_cb = example_free_hints_cb;
    esp_linenoise_config.read_bytes_cb = NULL; // use default read function
    esp_linenoise_config.write_bytes_cb = NULL; // use default write function
    esp_linenoise_config.history = NULL;
    ESP_ERROR_CHECK(esp_linenoise_create_instance(&esp_linenoise_config, &esp_linenoise_hdl));

    // Initialize command set (optional)
    const char* cmd_set[1] = { "cmd" };
    esp_cli_command_set_handle_t esp_cli_commands_cmd_set = ESP_CLI_COMMANDS_CREATE_CMD_SET(cmd_set, ESP_CLI_COMMAND_FIELD_ACCESSOR(name));

    esp_cli_config_t cli_cfg = {
        .linenoise_handle = esp_linenoise_hdl,
        .command_set_handle = esp_cli_commands_cmd_set, /* optional */
        .max_cmd_line_size = EXAMPLE_COMMAND_MAX_LENGTH,
        .history_save_path = "/spiffs/cli_history.txt", /* optional */
    };

    ret = esp_cli_create(&cli_cfg, &cli);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create esp_cli instance (%s)", esp_err_to_name(ret));
        return;
    }

    // Create esp_cli instance task
    if (xTaskCreate(example_cli_task, "example_cli_task", 4096, cli, 5, NULL) != pdPASS) {
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
