# 1-Wire Bus Programming Guide

The 1-Wire bus driver provides a generic interface for communicating with Dallas/Maxim 1-Wire devices on ESP chips. It hides the low-level timing requirements of the protocol behind a small, backend-agnostic API.

The documentation is split into two parts:

- **[1-Wire Bus Basics](1-wire-basics.md)** — how the bus actually works: electrical structure, reset/presence, read/write time slots, ROM search, CRC8 and parasite power. Read this first; it makes every API call obvious.
- **[Quick Start Guide](quick-start.md)** — the step-by-step recipe for using the component: create a bus, discover devices, talk to them, verify data and release resources.

## Overview

1-Wire is a device communications bus system that uses a single data line plus ground for communication — no separate clock line. Common 1-Wire devices include temperature sensors (DS18B20), EEPROMs (DS2431) and real-time clocks (DS2417).

This driver provides:

- Automatic 1-Wire bus initialization with an RMT or UART backend
- Device discovery and enumeration on the bus
- Read/write operations at bit and byte level
- Built-in CRC8 calculation for data integrity
- Support for multiple devices on a single bus

## API Usage Workflow

The diagram below shows the typical lifecycle of an application built on the 1-Wire bus driver. Each color highlights a different stage of the workflow: **bus setup**, **device discovery**, **data exchange**, and **resource cleanup**. Discovery can end as soon as ROM addresses are saved; data exchange may then repeat until the application is finished with the bus.

```mermaid
flowchart TD
    start(["Start"]):::entry

    subgraph SETUP["1 · Bus Setup"]
        direction TB
        choose{"Which peripheral<br/>do you use?"}:::decision
        rmt["onewire_new_bus_rmt()<br/>RMT backend"]:::setup
        uart["onewire_new_bus_uart()<br/>UART backend"]:::setup
    end

    subgraph DISCOVERY["2 · Device Discovery"]
        direction TB
        iter["onewire_new_device_iter()"]:::discovery
        enum["onewire_device_iter_get_next()<br/>read 64-bit ROM address"]:::discovery
        address["Keep the device address<br/>for later addressing"]:::discovery
        del_iter["onewire_del_device_iter()"]:::cleanup
    end

    subgraph EXCHANGE["3 · Data Exchange"]
        direction TB
        reset["onewire_bus_reset()<br/>presence check"]:::exchange
        match["Send ONEWIRE_CMD_MATCH_ROM<br/>+ address (optional)"]:::exchange
        write["onewire_bus_write_bytes()"]:::exchange
        read["onewire_bus_read_bytes()"]:::exchange
        crc["onewire_crc8()<br/>optional, application-side"]:::validate
        more{"Another transaction?"}:::decision
    end

    subgraph CLEANUP["4 · Cleanup"]
        direction TB
        del_bus["onewire_bus_del()"]:::cleanup
        end_node(["Done"]):::entry
    end

    start --> choose
    choose -- "RMT" --> rmt
    choose -- "UART" --> uart
    rmt --> iter
    uart --> iter
    iter --> enum --> address --> del_iter --> reset
    reset --> match --> write --> read --> crc --> more
    more -- "yes" --> reset
    more -- "no" --> del_bus --> end_node

    classDef entry fill:#4c6ef5,stroke:#364fc7,stroke-width:2px,color:#ffffff
    classDef setup fill:#d0ebff,stroke:#1c7ed6,stroke-width:1.5px,color:#0b3d66
    classDef discovery fill:#fff3bf,stroke:#f08c00,stroke-width:1.5px,color:#7a4b00
    classDef exchange fill:#d3f9d8,stroke:#2f9e44,stroke-width:1.5px,color:#1b4332
    classDef validate fill:#e5dbff,stroke:#7048e8,stroke-width:1.5px,color:#3b1f80
    classDef cleanup fill:#ffe3e3,stroke:#e03131,stroke-width:1.5px,color:#7a1a1a
    classDef decision fill:#ffffff,stroke:#495057,stroke-width:1.5px,color:#212529
```


## Add the Component to Your Project

Add the `onewire_bus` component to your project via the ESP Component Registry:

```bash
idf.py add-dependency "espressif/onewire_bus"
```

Then continue with the [Quick Start Guide](quick-start.md), or dive straight into the [1-Wire Bus Basics](1-wire-basics.md) chapter if you want to understand what happens on the wire first.

## Where to Go Next

- **[1-Wire Bus Basics](1-wire-basics.md)** — protocol principles, timing diagrams, ROM search, CRC8, parasite power and how the ESP backends generate the waveforms.
- **[Quick Start Guide](quick-start.md)** — bus creation, device enumeration, reads/writes and CRC verification with complete code snippets.
- **[API Reference](api.md)** — generated reference for every public function, type and macro.
