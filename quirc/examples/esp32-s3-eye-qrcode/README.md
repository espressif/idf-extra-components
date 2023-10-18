| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ------ | ----- | -------- | -------- | -------- | -------- | -------- | -------- |
|  |  |  |  |  |  |  | X|
# ESP32-S3-EYE QR-CODE with QUIRC

In this example the [ESP32-S3-EYE](https://github.com/espressif/esp-who/blob/master/docs/en/get-started/ESP32-S3-EYE_Getting_Started_Guide.md) takes a picture every few ms, show them on the display and check if the image contains valid qr-codes. If detected, it shows a message on the terminal (e.g. `Data: This is an esp-eye test`). It may not decode correctly the qr-code and in this case an error message is shown (`DECODE FAILED: ECC failure`).

Quirc requires a grayscale image, hence the picture is directly taken grayscale. Since the `lvgl` expects a color picture, the grayscale image is converted to RGB565 before giving it to the `lv_canvas_set_buffer`. The conversion is done by setting all three RGB values to the grayscale value (reduced to 5 or 6 bit respectively).

## How to use example

Build and flash on an esp32-s3-eye. Start a monitor and watch for decoding messages. It may work with small changes also on ESP32-CAM and ESP-EYE. 
