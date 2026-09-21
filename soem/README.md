# SOEM for ESP-IDF

This component ports [SOEM](https://github.com/OpenEtherCATsociety/SOEM), the Simple Open EtherCAT Master library, to ESP-IDF.

The component is based on SOEM v2.0.0 and uses the ESP-IDF Ethernet driver to transmit and receive raw EtherCAT frames. The application owns the Ethernet driver and must not run an IP stack on the same port.

Requires ESP-IDF 6.0 or later.

For SOEM protocol details, use the upstream [SOEM documentation](https://docs.rt-labs.com/soem) and headers.

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

## Configuration

Open **SOEM Configuration** in `idf.py menuconfig` to set resource limits and timeouts.

`EC_MAXSLAVE` defaults to 32 (range 2–200). Index 0 in SOEM's slave list is reserved for broadcast, so the minimum usable value is 2 (one real slave). Lower this on memory-constrained targets; the SOEM context is allocated on the heap.

Concurrent mapping is intentionally disabled (`EC_MAX_MAPT` is fixed to `1`) following [upstream SOEM maintainer guidance](https://github.com/OpenEtherCATsociety/SOEM/issues/535), values > 1 have known race conditions.

## Scope and limitations

What is compiled into the component is not automatically what has been validated on ESP-IDF.

### Validated

The [ecat_io](examples/ecat_io) example on ESP32-P4 (internal EMAC) has been tested with one digital-IO slave:

- Raw EtherCAT frame transmit and receive through `esp_eth`
- Slave discovery with `ecx_config_init()`
- INIT → PRE-OP → SAFE-OP → OP
- Process-data exchange with cyclic LRW

Cyclic timing in the example uses `esp_timer`. That is a functional period, not a hard real-time or Distributed Clocks guarantee.

### Compiled but not validated

These SOEM modules are built. The APIs exist, but they have not been verified on ESP-IDF in this port:

- Multiple slaves on one network
- Automatic PDO mapping (`ecx_config_map_group`) from a complete SII
- Mailbox protocols: CoE/SDO, FoE, EoE, SoE
- Distributed Clocks
- The IOmap path (`ecx_send_processdata` / `ecx_receive_processdata`)
- ENI-based configuration
- Slave recovery / hot-connect
- Multi-threaded use (cyclic I/O and mailbox transfer at same time)
- Ethernet drivers other than ESP32-P4 internal EMAC

### Not supported

- Redundant NICs (`ecx_init_redundant`)
- Hard real-time cycle guarantees
- Concurrent mapping threads

## Example

The [ecat_io](examples/ecat_io) example demonstrates slave discovery, state transitions and cyclic process-data communication on ESP32-P4.

> [!NOTE]
> Simple slaves without a microcontroller enable ESC device emulation
> (`0x0141[0] = 1`). For those devices the master must not leave the
> AL Status Error/Ack bit set. `ecx_config_init()` writes `PRE-OP | ACK`
> and does not wait. Request plain PRE-OP afterwards and wait until the
> slave state is exactly `EC_STATE_PRE_OP` (no error bit). See ETG ESC
> documentation, Device emulation (ESM emulation).

## License

The upstream SOEM library and adapted OSAL/OSHW files are dual-licensed under GPLv3 and a commercial license. Using this component in a commercial product typically requires a SOEM commercial license from RT-Labs.

The independently developed ESP-IDF wrapper (`esp_soem.c`, `esp_soem.h`) and example are licensed under the Apache License 2.0.

See [LICENSE](LICENSE) for details.
