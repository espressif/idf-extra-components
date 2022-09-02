# USB DTE plugin for esp_modem

> :warning: **Experimental feature**: USB DTE is under development!

This component extends [esp_modem](https://components.espressif.com/component/espressif/esp_modem) with USB DTE.

## Examples
For example usage see esp_modem examples: 
 * [console example](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples/modem_console)
 * [PPPoS example](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples/pppos_client)

## USB hotplugging and reconnection
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
