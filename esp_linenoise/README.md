# esp_linenoise (Multi-Instance Line Editor)

`esp_linenoise` is an enhanced version of the [linenoise](https://github.com/antirez/linenoise) line editing library, adapted for Espressif's IDF Extra Components ecosystem. It now supports **multi-instance usage**, making it ideal for applications that require multiple input contexts—such as multiple UART consoles, shells, or network sessions.

## ✨ Features

- Independent instances
- Configurable history, prompt, and line-editing behavior per instance
- Support for completion and hint callbacks
- IDF-style memory and error handling

## 🛠️ Usage Example

```c
esp_linenoise_config_t config;
esp_linenoise_get_instance_config_default(&config);
esp_linenoise_handle_t handle;
const esp_err_t ret = esp_linenoise_create_instance(&config, &handle);
const size_t buffer_size = 128;
char buffer[128];
const char *line = esp_linenoise_get_line(&handle, buffer, buffer_size);
const esp_err_t ret_val = esp_linenoise_delete_instance(&handle);
```

## 📚 API Highlights

| Function | Description |
|----------|-------------|
| `esp_linenoise_create_instance()` / `esp_linenoise_delete_instance()` | Create or destroy a linenoise |
| `esp_linenoise_get_line()` | Read a line from a given instance |

The user can pass to the configuration structure or set via setter functions, custom read and write functions
that will used by esp_linenoise in place of the default read / write.

The user can provide a custom set of file descriptors that esp_linenoise will use in place of the default
standard input file descriptors (STDIN_FILENO, STDOUT_FILENO).

For full API details, see [`esp_linenoise.h`](https://github.com/espressif/idf-extra-components/blob/master/esp_linenoise/include/esp_linenoise.h).

## 🧪 Build & Test

For detailed information concerning the integration of idf components into an idf project, please refer to [esp component manager documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## 📄 License

Apache 2.0. See `LICENSE` file.
