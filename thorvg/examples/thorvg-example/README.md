# Thorvg example

This is a minimalistic display + thorvg graphics library example.
In few function calls it sets up the display and shows Lottie animations.

## Building and running

Run the application as usual for an ESP-IDF project. For example, for ESP32P4:
```
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

## Hardware Required

esp32_p4_function_ev_board.


## Example output

The example should output the following:

```
I (1633) main_task: Calling app_main()
I (1690) ESP32_P4_EV: Partition size: total: 956561, used: 90109
I (1691) ESP32_P4_EV: MIPI DSI PHY Powered on
I (1692) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (1697) ili9881c: ID1: 0x98, ID2: 0x81, ID3: 0x5c
I (1746) ESP32_P4_EV: Install MIPI DSI LCD data panel
I (1771) ESP32_P4_EV: Display initialized
I (1772) ESP32_P4_EV: Setting LCD backlight: 100%
I (1799) main_task: Returned from app_main()
I (1815) example: set 1.000000 / 48.000000
I (1866) example: set 2.000000 / 48.000000
I (1915) example: set 3.000000 / 48.000000
I (1964) example: set 4.000000 / 48.000000
I (2013) example: set 5.000000 / 48.000000
I (2061) example: set 6.000000 / 48.000000
I (2109) example: set 7.000000 / 48.000000
I (2157) example: set 8.000000 / 48.000000
I (2205) example: set 9.000000 / 48.000000
I (2254) example: set 10.000000 / 48.000000
I (2303) example: set 11.000000 / 48.000000
I (2352) example: set 12.000000 / 48.000000
I (2401) example: set 13.000000 / 48.000000
I (2450) example: set 14.000000 / 48.000000
I (2500) example: set 15.000000 / 48.000000
I (2549) example: set 16.000000 / 48.000000
I (2597) example: set 17.000000 / 48.000000
I (2645) example: set 18.000000 / 48.000000
I (2693) example: set 19.000000 / 48.000000
I (2742) example: set 20.000000 / 48.000000
I (2792) example: set 21.000000 / 48.000000
I (2841) example: set 22.000000 / 48.000000
I (2898) example: set 23.000000 / 48.000000
I (2956) example: set 24.000000 / 48.000000
I (3012) example: set 25.000000 / 48.000000
I (3068) example: set 26.000000 / 48.000000
I (3124) example: set 27.000000 / 48.000000
I (3180) example: set 28.000000 / 48.000000
I (3237) example: set 29.000000 / 48.000000
I (3294) example: set 30.000000 / 48.000000
I (3350) example: set 31.000000 / 48.000000
I (3405) example: set 32.000000 / 48.000000
I (3462) example: set 33.000000 / 48.000000
I (3518) example: set 34.000000 / 48.000000
I (3569) example: set 35.000000 / 48.000000
I (3624) example: set 36.000000 / 48.000000
I (3679) example: set 37.000000 / 48.000000
I (3734) example: set 38.000000 / 48.000000
I (3789) example: set 39.000000 / 48.000000
I (3845) example: set 40.000000 / 48.000000
I (3899) example: set 41.000000 / 48.000000
I (3952) example: set 42.000000 / 48.000000
I (4006) example: set 43.000000 / 48.000000
I (4059) example: set 44.000000 / 48.000000
I (4113) example: set 45.000000 / 48.000000
I (4166) example: set 46.000000 / 48.000000
I (4216) example: set 47.000000 / 48.000000
I (4265) example: set 48.000000 / 48.000000
I (4314) example: CPU:86%, FPS:19/20
```
