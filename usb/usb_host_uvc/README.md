# USB Host UVC Driver

This directory contains USB host UVC driver based on [libuvc](https://github.com/libuvc/libuvc) library. Support for `libuvc` is achieved by implementing adapter between [libusb](https://github.com/libusb/libusb) (underling host library used by `libuvc`) and [usb_host](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/usb_host.html) library targeted for ESP SOCs.    

## Usage

Reference [uvc_host_example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/uvc) is similar to one found in `libuvc` repository with few additions:
1. Before calling `uvc_init()`, `initialize_usb_host_lib()` has to be called in order to initialize usb host library.
2. Since `libuvc` selects highest possible `dwMaxPayloadTransferSize` by default, user has to manually overwrite obatained value to 512 bytes (maximum transfer size supported by ESP32-S2/S3) before passing it to `uvc_print_stream_ctrl()` function.
3. Optionally, user can configure `libusb adapter` by passing appropriate parameters to `libuvc_adapter_set_config()`.

## Known limitations

Having only Full Speed USB peripheral and hardware limited MPS (maximum packet size) to 512 bytes, ESP32-S2/S3 is capable of reading about 0.5 MB of data per second. When connected to Full Speed USB host, cameras normally provide resolution no larger than 640x480 pixels. 
Following two supported formats are the most common (both encoded in MJPEG):
 * 320x240  30 FPS
 * 640x480  15 FPS

## Tested cameras
 * Logitech C980
 * CANYON CNE-CWC2
