# ESP-IDF CANopenNode Component

[![Component Registry](https://components.espressif.com/components/espressif/esp_canopennode/badge.svg)](https://components.espressif.com/components/espressif/esp_canopennode)

CANopen protocol implementation for ESP-IDF based on CANopenNode.

## Key Features

- Full CANopen protocol support (CiA 301)
- Object Dictionary (OD) management
- Process Data Objects (PDO)
- Service Data Objects (SDO)
- Heartbeat producer/consumer
- SYNC producer/consumer
- Emergency (EMCY) messages
- Node Guarding

> [!NOTE]
> This component is currently under development.

## Installation

To add this component to your project, run the following command from your project's root directory:

```bash
idf.py add-dependency espressif/esp_canopennode
```

## Quick Start Guide

> [!WARNING]
> This is a placeholder. Full implementation coming soon.

```c
#include "CANopen.h"

void app_main(void) {
    // TODO: Add board-specific CAN init and call standard CANopenNode APIs.
}
```

## Status

This component is in early development. The basic structure is in place, but full functionality is not yet available.
