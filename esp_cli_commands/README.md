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

## Usage Examples

For real-world usage and demonstration, see the following example projects in this repository:

- [hello_command_static](examples/hello_command_static): Static registration and API usage
- [math_op_static](examples/math_op_static): Static registration, argument parsing, error handling
- [debug_and_unregister_dynamic](examples/debug_and_unregister_dynamic): Dynamic registration and command lifecycle
- [command_set_example](examples/command_set_example): Command set creation, filtering, and concatenation

You can find these in the `esp_cli_commands/examples/` directory. Each example contains a README and complete source code. These examples are the recommended starting point for learning how to use this component in your own project.

---

## API Reference

- **Configuration**: `esp_cli_commands_update_config()`
- **Registration**: `esp_cli_commands_register_cmd()`, `esp_cli_commands_unregister_cmd()`
- **Execution**: `esp_cli_commands_execute()`, `esp_cli_commands_find_command()`
- **Completion & Help APIs**: `esp_cli_commands_get_completion()`, `esp_cli_commands_get_hint()`, `esp_cli_commands_get_glossary()`
- **Command Sets**: `esp_cli_commands_create_cmd_set()`, `esp_cli_commands_concat_cmd_set()`, `esp_cli_commands_destroy_cmd_set()`

---
