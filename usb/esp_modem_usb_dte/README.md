# USB DTE plugin for esp_modem

[![Component Registry](https://components.espressif.com/components/espressif/esp_modem_usb_dte/badge.svg)](https://components.espressif.com/components/espressif/esp_modem_usb_dte)

> :warning: **Experimental feature**: USB DTE is under development!

This component extends [esp_modem](https://components.espressif.com/component/espressif/esp_modem) with USB DTE.

## Examples
For example usage see esp_modem examples: 
 * [console example](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples/modem_console)
 * [PPPoS example](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples/pppos_client)

## USB hot-plugging and reconnection
USB DTE supports device reconnection and hot-plugging. You must only register callback function for `DEVICE_GONE` event and react accordingly:

C++ with lambda callback:
```cpp
auto dte = create_usb_dte(&dte_config);
dte->set_error_cb([&](terminal_error err) {
    if (err == terminal_error::DEVICE_GONE) {
        // Signal your application that USB modem device has been disconnected
    }
});
```

C:
```c
static void usb_terminal_error_handler(esp_modem_terminal_error_t err) {
    if (err == ESP_MODEM_TERMINAL_DEVICE_GONE) {
        // Signal your application that USB modem device has been disconnected
    }
}
esp_modem_dce_t *dce = esp_modem_new_dev_usb(ESP_MODEM_DCE_BG96, &dte_usb_config, &dce_config, esp_netif);
esp_modem_set_error_cb(dce, usb_terminal_error_handler);
```

## Dual port modems
@todo

## Adding a new modem
For simple cases with one AT port, you should be able to open communication with the modem by defining:
1. **USB VID and PID:** This can be found by plugging the modem to a PC and running `lsusb -v` on Linux or by [USB Device Tree Viewer](https://www.uwe-sieber.de/usbtreeview_e.html) on Windows.
2. **AT port interface number:** USB modem have usually multiple CDC-ACM-like ports. One (or two) is dedicated for AT commands. You can find the correct interface number based on its string descriptor (using methods from point 1), from the modem's datasheet or by trial and error.

Then, you can pass these constants to [ESP_MODEM_DEFAULT_USB_CONFIG](https://github.com/espressif/idf-extra-components/blob/master/usb/esp_modem_usb_dte/include/esp_modem_usb_config.h#L47) macro and start testing AT commands.

## List of tested modems
The following modems were tested with this component, their configurations can be found in [esp_modem_usb_config.h](https://github.com/espressif/idf-extra-components/blob/master/usb/esp_modem_usb_dte/include/esp_modem_usb_config.h):
* Quactel BG96
* SimCom SIM7600E
* SimCom A7670E
