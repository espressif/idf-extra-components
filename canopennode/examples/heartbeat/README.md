# CANopenNode Heartbeat Example

This example demonstrates the **pure application-layer usage** of CANopenNode protocol stack to send heartbeat messages. It assumes that the underlying hardware driver (esp_twai) has already been adapted to support CANopenNode.

* ! NOTICE   
Don't forgot below code in project CMakeLists.txt to provide OD during build the project

```
# Add object dictionary path where the OD.h and OD.c files are located
idf_build_set_property(INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/main" APPEND)
```

## Testing with SocketCAN

`test_canopen.py` verifies NMT state transitions via SocketCAN. It requires a PC connected to the same CAN bus (e.g. via a USB-CAN adapter).

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
