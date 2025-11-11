# ESP Commands

The `esp_cli_commands` component provides a flexible command registration and execution framework for ESP-IDF applications.  
It allows applications to define console-like commands with metadata (help text, hints, glossary entries) and register them dynamically or statically.

---

## Features

- Define commands with:
  - Command name
  - Group categorization
  - Help text
  - Optional hints and glossary callbacks
- Register commands at runtime or at compile-time (via section placement macros).
- Execute commands from command line strings.
- Provide command completion, hints, and glossary callback registration mechanism.
- Create and manage subsets of commands (command sets).
- Customizable configuration for command parsing and hint display.

---

## Configuration

By default, the component is initialized with a default configuration. It is however possible for the user to update this configuration with the call of the following API:

```c
esp_cli_commands_config_t config = {
    .heap_caps_used = <user specific value>,
    .max_cmdline_length = <user specific value>,
    .max_cmdline_args = <user specific value>,
    .hint_color = <user specific value>,
    .hint_bold = <user specific value>
};
esp_cli_commands_update_config(&config);
```

- `write_func`: The custom write function used by esp_cli_commands to output data (default to posix write is not specified)
- `max_cmdline_length`: Maximum command line buffer length (bytes).
- `max_cmdline_args`: Maximum number of arguments parsed.
- `hint_color`: ANSI color code used for hints.
- `hint_bold`: Whether hints are displayed in bold.

---

## Defining Commands

### Command Structure

A command is described by the `esp_cli_command_t` struct:

```c
typedef struct esp_cli_command {
    const char *name;                     /*!< Command name */
    const char *group;                    /*!< Group/category */
    const char *help;                     /*!< Short help text */
    esp_cli_command_func_t func;              /*!< Command implementation */
    void *func_ctx;                       /*!< User context */
    esp_cli_command_hint_t hint_cb;           /*!< Hint callback */
    esp_cli_command_glossary_t glossary_cb;   /*!< Glossary callback */
} esp_cli_command_t;
```

### Static Registration

Use the `ESP_CLI_COMMAND_REGISTER` macro to register a command at compile time:

```c
static int my_cmd(void *context, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv) {
    printf("Hello from my_cmd!\n");
    return 0;
}

ESP_CLI_COMMAND_REGISTER(my_cmd, tools, "Prints hello", my_cmd, NULL, NULL, NULL);
```

This places the command into the `.esp_cli_commands` section in flash.

### Dynamic Registration

Commands can also be registered/unregistered at runtime:

```c
esp_cli_command_t cmd = {
    .name = "my_cmd",
    .group = "tools",
    .help = "Prints hello",
    .func = my_cmd,
};

esp_cli_commands_register_cmd(&cmd);
esp_cli_commands_unregister_cmd("echo");
```

---

## Executing Commands

Commands can be executed from a command line string:

```c
int cmd_ret;
esp_err_t ret = esp_cli_commands_execute("my_cmd arg1 arg2", &cmd_ret, NULL, STDOUT_FILENO);
```

- `cmd_set`: Limits execution to a set of commands (or `NULL` for all commands).
- `cmd_fd`: the file descriptor on which the output of the command is directed
- `cmd_line`: String containing the command and arguments.
- `cmd_ret`: Receives the command function return value.

---

## Command Completion, Hints, and Glossary

Completion & Help APIs:

```c
esp_cli_commands_get_completion(NULL, "ec", completion_cb);
const char *hint = esp_cli_commands_get_hint(NULL, "echo", &color, &bold);
const char *glossary = esp_cli_commands_get_glossary(NULL, "echo");
```

- **Completion**: Suggests matching commands.
- **Hint**: Provides a short usage hint.
- **Glossary**: Provides detailed command argument description.

---

## Command Sets

Command sets allow grouping subsets of commands for filtering:

```c
const char *cmd_names[] = {"echo", "my_cmd"};
esp_cli_command_set_handle_t set =
    ESP_CLI_COMMANDS_CREATE_CMD_SET(cmd_names, ESP_CLI_COMMAND_FIELD_ACCESSOR(name));

esp_cli_commands_execute("my_cmd", NULL, set, NULL);
esp_cli_commands_destroy_cmd_set(&set);
```

- Create sets by name, group, or other fields.
- Concatenate sets with `esp_cli_commands_concat_cmd_set()`.
- Destroy sets when no longer needed.

---

## Quick Start Example

```c
#include <stdio.h>
#include "esp_cli_commands.h"

// Example command function
static int hello_cmd(void *ctx, int argc, char **argv) {
    printf("Hello, ESP Commands!\n");
    return 0;
}

// Register command statically
ESP_CLI_COMMAND_REGISTER(hello_cmd, demo, "Prints a hello message", hello_cmd, NULL, NULL, NULL);

void app_main(void) {
    // Update configuration (optional)
    esp_cli_commands_config_t config = {
        .heap_caps_used = MALLOC_CAP_INTERNAL,
        .max_cmdline_length = 64,
        .max_cmdline_args = 4,
        .hint_color = 31, // Red foreground
        .hint_bold = true
    };
    esp_cli_commands_update_config(&config);

    // Execute command
    int ret_val;
    esp_err_t ret = esp_cli_commands_execute("hello_cmd", &ret_val, NULL, NULL);
    if (ret == ESP_OK) {
        printf("Command executed successfully, return value: %d\n", ret_val);
    } else {
        printf("Failed to execute command, error: %d\n", ret);
    }
}
```

---

## API Reference

- **Configuration**: `esp_cli_commands_update_config()`
- **Registration**: `esp_cli_commands_register_cmd()`, `esp_cli_commands_unregister_cmd()`
- **Execution**: `esp_cli_commands_execute()`, `esp_cli_commands_find_command()`
- **Completion & Help APIs**: `esp_cli_commands_get_completion()`, `esp_cli_commands_get_hint()`, `esp_cli_commands_get_glossary()`
- **Command Sets**: `esp_cli_commands_create_cmd_set()`, `esp_cli_commands_concat_cmd_set()`, `esp_cli_commands_destroy_cmd_set()`

---
