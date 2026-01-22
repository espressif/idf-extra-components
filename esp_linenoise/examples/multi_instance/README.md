# esp_linenoise Multi-Instance Example

This example demonstrates how to use multiple esp_linenoise instances:
- Create separate instances with different configurations
- Maintain independent history for each instance
- Switch between instances at runtime
- Save separate history files for each context

## How it works
- Two instances are created: "user" and "admin"
- Type commands in either mode
- Type 'switch' to toggle between user and admin mode
- Each mode maintains its own command history
- Type 'exit' to quit

## Build & Run
See the top-level README for build instructions. This example is portable for ESP-IDF and Linux.
