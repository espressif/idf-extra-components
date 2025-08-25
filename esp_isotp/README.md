# ESP-IDF ISO-TP Component

[![Component Registry](https://components.espressif.com/components/espressif/esp_isotp/badge.svg)](https://components.espressif.com/components/espressif/esp_isotp)

This component implements ISO 15765-2 (ISO-TP) protocol for transmitting large data payloads over CAN/TWAI networks, enabling automatic segmentation and reassembly of messages up to 4095 bytes.

## Features

- **Single-frame transmission** (≤7 bytes): Immediate transmission in one TWAI frame
- **Multi-frame transmission** (>7 bytes): Automatic segmentation with flow control
- **ISR-based frame reception** for low-latency processing
- **Non-blocking API** suitable for real-time applications  
- **Multi-instance support** for handling multiple CAN IDs simultaneously
- **Callback support** for transmission and reception completion events
- **Automotive compliance** with UDS, OBD-II, and other diagnostic protocols

The component handles all ISO-TP protocol complexities internally, requiring only regular calls to `esp_isotp_poll()` to drive the state machine.

## Error Handling

- **Timeout detection** for incomplete transmissions and receptions
- **Sequence validation** to ensure frame ordering
- **Buffer overflow protection** with configurable limits  
- **Flow control** management for reliable multi-frame transfers

## Installation

```bash
idf.py add-dependency espressif/esp_isotp
```

## Quick Start

```c
#include "esp_isotp.h"
#include "esp_twai_onchip.h"

// Initialize TWAI
twai_onchip_node_config_t twai_cfg = {
    .io_cfg = {.tx = GPIO_NUM_5, .rx = GPIO_NUM_4},
    .bit_timing = {.bitrate = 500000},
    .tx_queue_depth = 16,
};
twai_node_handle_t twai_node;
ESP_ERROR_CHECK(twai_new_node_onchip(&twai_cfg, &twai_node));

// Configure ISO-TP
esp_isotp_config_t config = {
    .tx_id = 0x7E0,           // TWAI ID for transmission
    .rx_id = 0x7E8,           // TWAI ID for reception
    .tx_buffer_size = 4096,   // Maximum message size to send
    .rx_buffer_size = 4096,   // Maximum message size to receive
};

// Create ISO-TP transport
esp_isotp_handle_t isotp_handle;
ESP_ERROR_CHECK(esp_isotp_new_transport(twai_node, &config, &isotp_handle));

// Communication loop
while (1) {
    // CRITICAL: Must call every 1-10ms to process frames
    esp_isotp_poll(isotp_handle);
    
    // Non-blocking send/receive operations
    esp_isotp_send(isotp_handle, data, size);
    esp_isotp_receive(isotp_handle, buffer, buffer_size, &received_size);
    
    vTaskDelay(pdMS_TO_TICKS(5));
}
```

## Examples

- **[echo](examples/echo/)** - Simple echo server demonstrating basic ISO-TP communication

## Documentation

For detailed information about the ISO-TP component, including API reference and advanced configuration:

- **Programming Guide & API Reference**: [ISO-TP Documentation](https://espressif.github.io/idf-extra-components/latest/esp_isotp/index.html)