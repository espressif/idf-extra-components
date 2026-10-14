# Lua Blink Example

This example shows how to embed Lua in an ESP-IDF application.

It creates one long-lived `lua_State`, exposes firmware APIs as C modules (`esp`, `led_strip`) that the script loads with `require()`, and runs [`assets/blink_led.lua`](assets/blink_led.lua) from LittleFS. That script also `require()`s a Lua module from the same directory ([`assets/colors.lua`](assets/colors.lua)) and blinks one LED on a WS2812 strip.

The C bindings live in [`main/app_lua.c`](main/app_lua.c). [`main/lua_blink_main.c`](main/lua_blink_main.c) only mounts the filesystem, creates the state, preloads the modules and runs the script.

Scripts load the C modules with `require()`, the same way they load a `.lua` file:

```lua
local esp = require("esp")
local led_strip = require("led_strip")

esp.free_heap()                 -- free heap in bytes
esp.sleep_ms(ms)                -- yield the firmware task for `ms` milliseconds
led_strip.led_count             -- number of pixels (integer field)
led_strip.set_pixel(i, r, g, b) -- set one pixel; `i` < led_count, RGB in 0..255
led_strip.refresh()             -- flush the pixel buffer to the strip
```

Wrong types or out-of-range arguments raise a Lua error. Hardware failures return `false` plus an `esp_err` name.

## How to Use Example

### Hardware Required

* An ESP32-series development board with at least 4 MB flash
* A USB cable for power and serial
* A WS2812 (or compatible) LED strip

GPIO and strip length are `LED_STRIP_GPIO_PIN` / `LED_STRIP_LED_COUNT` in [`main/app_lua.c`](main/app_lua.c).

### Build and Flash

```bash
idf.py set-target <chip_name>
idf.py build flash monitor
```

To exit the serial monitor, type `Ctrl-]`.

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF.

## Example Output

The first LED of the strip blinks five times. The console looks like this (heap size and timestamps vary):

```
I (xxx) lua_blink: Lua Blink Example Starting
I (xxx) lua_blink: Initializing LittleFS filesystem
I (xxx) lua_blink: Filesystem mounted at /assets
I (xxx) app_lua: Created LED strip object with RMT backend
I (xxx) lua_blink: Starting Lua script from file: blink_led.lua
Blinking LED 0 on the strip, 5 times
LED ON
LED OFF
LED ON
LED OFF
LED ON
LED OFF
LED ON
LED OFF
LED ON
LED OFF
Blinking finished, free heap: 199 KB
I (xxx) lua_blink: End of Lua script from file: blink_led.lua
I (xxx) lua_blink: End of Lua blink example application.
```

## See Also

- [Lua Reference Manual](https://www.lua.org/manual/)
