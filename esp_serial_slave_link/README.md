# Espressif Serial Slave Link (ESSL) component

[![Component Registry](https://components.espressif.com/components/espressif/esp_serial_slave_link/badge.svg)](https://components.espressif.com/components/espressif/esp_serial_slave_link)

This component used to reside in [esp-idf](https://github.com/espressif/esp-idf) project as its component.

It's used on the HOST, to communicate with ESP chips as SLAVE via SDIO/SPI slave HD mode.

The port layer (`essl_sdio.c/essl_spi.c`) are currently only written to run on ESP chips in master mode, but you may also modify them to work on more platforms.

See more documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_serial_slave_link.html
