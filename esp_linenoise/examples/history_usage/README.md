# esp_linenoise History Usage Example

This example demonstrates how to use esp_linenoise with input history:
- Enable and configure history length
- Load and save history to a file
- Add new entries to history after each input

## How it works
- Prompts the user for input with `history> `
- After each line, adds it to the history and saves to disk
- Loads history from disk on startup

## Build & Run
See the top-level README for build instructions. This example is portable for ESP-IDF and Linux.
