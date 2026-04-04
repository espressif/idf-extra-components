# Dallas 1-Wire Bus Driver

[![Component Registry](https://components.espressif.com/components/espressif/onewire_bus/badge.svg)](https://components.espressif.com/components/espressif/onewire_bus)

This directory contains an implementation for Dallas 1-Wire bus by different peripherals.
The following low-level backends are currently supported:

- RMT backend (`onewire_new_bus_rmt`)
- UART backend (`onewire_new_bus_uart`)

## Features

- Automatic 1-Wire bus initialization with RMT or UART backend
- Device discovery and enumeration on the bus
- Read/write operations at bit and byte level
- Built-in CRC8 calculation for data integrity
- Support for multiple devices on a single bus

## Add to Your Project

Add the `onewire_bus` component to your project via the ESP Component Registry:

```bash
idf.py add-dependency "espressif/onewire_bus"
```

## Documentation

- [Programming Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/bus/onewire_bus.html) - Complete guide with code examples, backend selection, device enumeration, and communication patterns
- [API Reference](https://docs.espressif.com/projects/esp-iot-solution/en/latest/api/onewire_bus.html) - Detailed API documentation

## Appendix

* [DS18B20 device driver based on the 1-Wire Bus driver](https://components.espressif.com/components/espressif/ds18b20) and the [DS18B20 Example](https://github.com/espressif/esp-bsp/tree/master/components/ds18b20/examples/ds18b20_read)
