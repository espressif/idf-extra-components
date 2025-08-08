# ISO-TP Protocol

![maintenance-status](https://img.shields.io/badge/maintenance-actively--developed-brightgreen.svg)

This component provides an implementation of the ISO 15765-2 (ISO-TP) protocol for the ESP-IDF framework, based on the `isotp-c` library. It allows for sending and receiving large data packets over the CAN bus.

## Features

- Full ISO-TP protocol support (single/multi-frame, flow control)
- ESP-IDF TWAI driver integration
- Large payload handling (up to 4095 bytes, limited by 12-bit length field)
- Linux can-utils compatibility
- QEMU testing support

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency espressif/isotp
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Basic Usage

```c
#include "esp_isotp.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

// Create TWAI node and ISO-TP configuration
twai_onchip_node_config_t twai_cfg = {
    .io_cfg = {.tx = GPIO_NUM_5, .rx = GPIO_NUM_4},
    .bit_timing = {.bitrate = 500000},
    .tx_queue_depth = 16,
};
twai_node_handle_t twai_node;
twai_new_node_onchip(&twai_cfg, &twai_node);

QueueHandle_t rx_queue = xQueueCreate(16, sizeof(isotp_rx_queue_item_t));

esp_isotp_config_t config = {
    .tx_id = 0x7E0, .rx_id = 0x7E8,
    .tx_buffer_size = 4096, .rx_buffer_size = 4096,
    .twai_node = twai_node, .rx_queue = rx_queue,
};
esp_isotp_handle_t isotp_handle = esp_isotp_new(&config);

// Send/receive data
uint8_t data[] = {0x22, 0xF1, 0x90};
esp_isotp_send(isotp_handle, data, sizeof(data));

esp_isotp_poll(isotp_handle);
uint8_t rx_buffer[4096];
int len = esp_isotp_receive(isotp_handle, rx_buffer, sizeof(rx_buffer));
```

## Configuration

The component uses automatic patch application during build to customize the upstream `isotp-c` library. Current patches:
- **Frame padding disabled**: `ISO_TP_FRAME_PADDING` is commented out to optimize payload usage
- **Response timeout**: Default is 100ms via `ISO_TP_DEFAULT_RESPONSE_TIMEOUT`

## Troubleshooting

- **Multi-instance conflicts**: The underlying `isotp-c` library is designed as a singleton. Avoid creating multiple `esp_isotp` instances simultaneously.
- **Large frame timeouts**: If you experience timeouts with large data frames, check the `ISO_TP_DEFAULT_RESPONSE_TIMEOUT` configuration in `isotp-c/isotp_config.h` (modified via patch during build) and ensure your network has sufficient bandwidth.
- **QEMU test failures**: Ensure the `vcan0` interface is available on your system and that the QEMU path and arguments are correct.
- **Patch application failures**: If you see warnings about patch application during build, the patch may already be applied. This is normal on subsequent builds.

For any technical queries, please open an [issue](https://github.com/espressif/idf-extra-components/issues) on GitHub.
