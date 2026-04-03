# Dallas 1-Wire Bus Driver

[![Component Registry](https://components.espressif.com/components/espressif/onewire_bus/badge.svg)](https://components.espressif.com/components/espressif/onewire_bus)

This directory contains an implementation for Dallas 1-Wire bus by different peripherals.
The following low-level backends are currently supported:

- RMT backend (`onewire_new_bus_rmt`)
- UART backend (`onewire_new_bus_uart`)

## Appendix

* [DS18B20 device driver based on the 1-Wire Bus driver](https://components.espressif.com/components/espressif/ds18b20) and the [DS18B20 Example](https://github.com/espressif/esp-bsp/tree/master/components/ds18b20/examples/ds18b20-read)
