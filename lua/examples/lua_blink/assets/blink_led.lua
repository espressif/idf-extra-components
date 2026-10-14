-- SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
--
-- SPDX-License-Identifier: Apache-2.0
--
-- blink_led.lua
-- Blinks a single LED of the strip. Modules come from require():
--   led_strip  C module: set_pixel / refresh
--   esp        C module: sleep_ms / free_heap
--   colors     Lua module from this same directory

local esp = require("esp")
local led_strip = require("led_strip")
local colors = require("colors")

local INDEX = 0       -- which LED to blink; must be < led_strip.led_count
local BLINK_COUNT = 5 -- how many ON/OFF cycles to run
local PERIOD_MS = 500 -- half period, i.e. ON for 500 ms then OFF for 500 ms

local function set_led(r, g, b)
    -- set_pixel / refresh return false plus an error string on failure
    local ok, err = led_strip.set_pixel(INDEX, r, g, b)
    if not ok then
        print('set_pixel failed: ' .. tostring(err))
        return false
    end
    ok, err = led_strip.refresh()
    if not ok then
        print('refresh failed: ' .. tostring(err))
        return false
    end
    return true
end

print('Blinking LED ' .. INDEX .. ' on the strip, ' .. BLINK_COUNT .. ' times')

for i = 1, BLINK_COUNT do
    -- ON: light the LED up with a moderate brightness
    if not set_led(colors.on.r, colors.on.g, colors.on.b) then
        break
    end
    print('LED ON')
    esp.sleep_ms(PERIOD_MS)

    -- OFF: clear the pixel back to black
    if not set_led(colors.off.r, colors.off.g, colors.off.b) then
        break
    end
    print('LED OFF')
    esp.sleep_ms(PERIOD_MS)
end

print('Blinking finished, free heap: ' .. math.floor(esp.free_heap() / 1024) .. ' KB')
