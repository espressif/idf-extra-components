## 1.0.4

- Support `en_pull_up` config option in `onewire_bus_config_t`, which can enable the internal pull-up resistor on the GPIO pin used for the one-wire bus. This is useful when using a GPIO pin that does not have a pull-up resistor connected externally.

## 1.0.3

- Improve the driver to support esp-idf v6.0

## 1.0.2

- raise recovery time to support more sensor on longer wire (d0b2b52)

## 1.0.0

- Initial driver version, with the RMT driver as backend controller
