# esp_linenoise Completion Example

This example demonstrates how to use esp_linenoise with tab-completion:
- Register a completion callback function
- Suggest command completions as the user types
- Complete commands with the TAB key

## How it works
- Type the beginning of a command (e.g., "h") and press TAB
- Available completions are displayed or auto-completed
- Commands: help, history, clear, exit, status, config, reset

## Build & Run
See the top-level README for build instructions. This example is portable for ESP-IDF and Linux.
