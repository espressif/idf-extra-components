# Command Set Example

This example demonstrates the use of command sets in esp_cli_commands:

- Two commands are created, each belonging to a different group.
- Two command sets are created, one for each group.
- Each command is executed with each set to test filtering.
- The sets are concatenated and both commands are executed with the combined set.
- All sets and commands are cleaned up at the end.

## Files
- main/command_set_main.c: Main example source code
- main/CMakeLists.txt: Build configuration
- main/idf_component.yml: ESP-IDF component manifest

## Usage
Build and flash as a standard ESP-IDF example. Output will show which commands are executed or filtered by the command set.
