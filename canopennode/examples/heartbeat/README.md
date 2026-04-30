# CANopenNode Heartbeat Example

This example demonstrates application-layer usage of the CANopenNode protocol
stack on ESP-IDF. It creates the TWAI node in the application, initializes
CANopenNode with the local Object Dictionary, and exercises heartbeat/NMT, SDO,
and asynchronous RPDO/TPDO paths.

The example uses TWAI TX GPIO 4, RX GPIO 5, node ID 1, and 200 kbit/s bitrate.

## Build Notes

The application provides `OD.c` and `OD.h` from `main/`. When
`CONFIG_CO_MULTIPLE_OD` is disabled, make `OD.h` visible to the `canopennode`
component from the project `CMakeLists.txt`:

```cmake
# Add the OD.h and OD.c files path for canopennode component
idf_component_get_property(canopennode_lib canopennode COMPONENT_LIB)
target_include_directories(${canopennode_lib} PRIVATE ${CMAKE_SOURCE_DIR}/main)
```

`sdkconfig.defaults` enables `CONFIG_CO_SDO_CLIENT` and `CONFIG_CO_PDO` so the
full self-test and SocketCAN test can run.

## Testing with SocketCAN

`test_canopen.py` verifies NMT state transitions, SDO access to object `0x1008`,
and asynchronous RPDO/TPDO transfer of `TestCNT` via SocketCAN. It requires a PC
connected to the same CAN bus, for example through a USB-CAN adapter.

Configure the SocketCAN interface for the same bitrate:

```bash
sudo ip link set can0 up type can bitrate 200000
```

Install the Python dependency and run the test:

```bash
pip install canopen
sudo python3 test_canopen.py
```

The script exercises the following sequence and checks each heartbeat state:

| Step | NMT command | Expected heartbeat state |
|------|-------------|--------------------------|
| 1    | —           | PRE-OPERATIONAL          |
| 2    | START (0x01) | OPERATIONAL             |
| 3    | ENTER PRE-OPERATIONAL (0x80) | PRE-OPERATIONAL |
| 4    | STOP (0x02) | STOPPED                  |
| 5    | RESET COMMUNICATION (0x82) | INITIALISING → PRE-OPERATIONAL |

If the SocketCAN interface is not `can0`, edit the `channel` argument in the script.
