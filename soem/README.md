# SOEM for ESP-IDF

This component ports [SOEM](https://github.com/OpenEtherCATsociety/SOEM), the Simple Open EtherCAT Master library, to ESP-IDF.

The component is based on SOEM v2.0.0 and uses the ESP-IDF Ethernet driver to transmit and receive raw EtherCAT frames.

## Add the component

```bash
idf.py add-dependency "espressif/soem^2.0.0"
```

Include the public header:

```c
#include "esp_soem.h"
```

## Basic usage

The application is responsible for initializing and starting the Ethernet driver.

Initialize SOEM with the Ethernet driver handle:

```c
ecx_contextt *context = esp_soem_init(eth_handle);
if (context == NULL) {
    /* Handle initialization failure. */
}
```

The returned context can be used with standard SOEM context APIs:

```c
int slave_count = ecx_config_init(context);
```

Stop Ethernet reception before releasing the SOEM context:

```c
ESP_ERROR_CHECK(esp_eth_stop(eth_handle));
esp_soem_deinit(context);
```

The Ethernet driver remains owned by the application.

## Configuration

SOEM resource limits and timeout values can be configured under **SOEM Configuration** in `idf.py menuconfig`.

## Example

The [ecat_io](examples/ecat_io) example demonstrates slave discovery, state transitions and cyclic process-data communication on ESP32-P4.

## License

The upstream SOEM library and adapted OSAL/OSHW files are dual-licensed under GPLv3 and a commercial license.

The independently developed ESP-IDF wrapper and example are licensed under the Apache License 2.0.

See [LICENSE](LICENSE) for details.
