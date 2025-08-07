# ESP Commands

The `esp_commands` component provides a flexible command registration and execution framework for ESP-IDF applications.  
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

The component is initialized with a configuration struct:

```c
esp_commands_config_t config = ESP_COMMANDS_CONFIG_DEFAULT();
esp_commands_update_config(&config);
```

- `max_cmdline_length`: Maximum command line buffer length (bytes).
- `max_cmdline_args`: Maximum number of arguments parsed.
- `hint_color`: ANSI color code used for hints.
- `hint_bold`: Whether hints are displayed in bold.

---

## Defining Commands

### Command Structure

A command is described by the `esp_command_t` struct:

```c
typedef struct esp_command {
    const char *name;                     /*!< Command name */
    const char *group;                    /*!< Group/category */
    const char *help;                     /*!< Short help text */
    esp_command_func_t func;              /*!< Command implementation */
    void *func_ctx;                       /*!< User context */
    esp_command_hint_t hint_cb;           /*!< Hint callback */
    esp_command_glossary_t glossary_cb;   /*!< Glossary callback */
} esp_command_t;
```

### Static Registration

Use the `ESP_COMMAND_REGISTER` macro to register a command at compile time:

```c
static int my_cmd(void *ctx, int argc, char **argv) {
    printf("Hello from my_cmd!\n");
    return 0;
}

ESP_COMMAND_REGISTER(my_cmd, tools, "Prints hello", my_cmd, NULL, NULL, NULL);
```

This places the command into the `.esp_commands` section.

### Dynamic Registration

Commands can also be registered/unregistered at runtime:

```c
esp_command_t cmd = {
    .name = "echo",
    .group = "utils",
    .help = "Echoes arguments back",
    .func = echo_func,
};

esp_commands_register_cmd(&cmd);
esp_commands_unregister_cmd("echo");
```

---

## Executing Commands

Commands can be executed from a command line string:

```c
int cmd_ret;
esp_err_t ret = esp_commands_execute(NULL, "my_cmd arg1 arg2", &cmd_ret);
```

- `cmd_set`: Limits execution to a set of commands (or `NULL` for all commands).
- `cmd_line`: String containing the command and arguments.
- `cmd_ret`: Receives the command function return value.

---

## Command Completion, Hints, and Glossary

Completion & Help APIs:

```c
esp_commands_get_completion(NULL, "ec", completion_cb);
const char *hint = esp_commands_get_hint(NULL, "echo", &color, &bold);
const char *glossary = esp_commands_get_glossary(NULL, "echo");
```

- **Completion**: Suggests matching commands.
- **Hint**: Provides a short usage hint.
- **Glossary**: Provides detailed command description.

---

## Command Sets

Command sets allow grouping subsets of commands for filtering:

```c
const char *cmd_names[] = {"echo", "my_cmd"};
esp_command_set_handle_t set =
    ESP_COMMANDS_CREATE_CMD_SET(cmd_names, FIELD_ACCESSOR(name));

esp_commands_execute(set, "echo Hello!", NULL);
esp_commands_destroy_cmd_set(&set);
```

- Create sets by name, group, or other fields.
- Concatenate sets with `esp_commands_concat_cmd_set()`.
- Destroy sets when no longer needed.

---

## Quick Start Example

```c
#include <stdio.h>
#include "esp_commands.h"

// Example command function
static int hello_cmd(void *ctx, int argc, char **argv) {
    printf("Hello, ESP Commands!\n");
    return 0;
}

// Register command statically
ESP_COMMAND_REGISTER(hello_cmd, demo, "Prints a hello message", hello_cmd, NULL, NULL, NULL);

void app_main(void) {
    // Update configuration (optional)
    esp_commands_config_t config = ESP_COMMANDS_CONFIG_DEFAULT();
    esp_commands_update_config(&config);

    // Execute command
    int ret_val;
    esp_err_t ret = esp_commands_execute(NULL, "hello_cmd", &ret_val);
    if (ret == ESP_OK) {
        printf("Command executed successfully, return value: %d\n", ret_val);
    } else {
        printf("Failed to execute command, error: %d\n", ret);
    }
}
```

---

## API Reference

- **Configuration**: `esp_commands_update_config()`
- **Registration**: `esp_commands_register_cmd()`, `esp_commands_unregister_cmd()`
- **Execution**: `esp_commands_execute()`, `esp_commands_find_command()`
- **Completion & Help APIs**: `esp_commands_get_completion()`, `esp_commands_get_hint()`, `esp_commands_get_glossary()`
- **Command Sets**: `esp_commands_create_cmd_set()`, `esp_commands_concat_cmd_set()`, `esp_commands_destroy_cmd_set()`

---
