# ESP-IDF ISO-TP Component

[![Component Registry](https://components.espressif.com/components/espressif/esp_isotp/badge.svg)](https://components.espressif.com/components/espressif/esp_isotp)

ISO 15765-2 (ISO-TP) transport protocol implementation for ESP-IDF, enabling reliable transmission of large data payloads (up to 4095 bytes) over TWAI networks with automatic segmentation and reassembly.

## Key Features

- **Automatic segmentation** for messages >7 bytes with flow control
- **Non-blocking API** with ISR-based frame processing
- **Multi-instance support** for concurrent communication channels
- **Automotive compliance** (UDS, OBD-II compatible)
- **Robust error handling** with timeout and sequence validation

> [!NOTE]
> TWAI-FD (Flexible Data-rate)  is not supported in this version.

## Installation

```bash
idf.py add-dependency espressif/esp_isotp
```

## Configuration

Configure ISO-TP protocol parameters:

```bash
idf.py menuconfig
# Navigate to: Component config → ISO-TP Protocol Configuration
```

## Quick Start

```c
#include "esp_isotp.h"
#include "esp_twai_onchip.h"

void app_main(void) {
    // 1. Initialize TWAI
    twai_onchip_node_config_t twai_cfg = {
        .io_cfg = {.tx = GPIO_NUM_5, .rx = GPIO_NUM_4},
        .bit_timing = {.bitrate = 500000},
        .tx_queue_depth = 16,
    };
    twai_node_handle_t twai_node;
    ESP_ERROR_CHECK(twai_new_node_onchip(&twai_cfg, &twai_node));

    // 2. Create ISO-TP transport
    esp_isotp_config_t config = {
        .tx_id = 0x7E0, .rx_id = 0x7E8,
        .tx_buffer_size = 4096, .rx_buffer_size = 4096,
    };
    esp_isotp_handle_t isotp_handle;
    ESP_ERROR_CHECK(esp_isotp_new_transport(twai_node, &config, &isotp_handle));

    // 3. Communication loop
    uint8_t buffer[4096];
    uint32_t received_size;

    while (1) {
        esp_isotp_poll(isotp_handle);  // CRITICAL: Call every 1-10ms

        if (esp_isotp_receive(isotp_handle, buffer, sizeof(buffer), &received_size) == ESP_OK) {
            printf("Received %lu bytes\n", received_size);
            esp_isotp_send(isotp_handle, buffer, received_size);  // Echo back
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```
