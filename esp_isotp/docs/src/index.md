# ISO-TP Programming Guide

## Introduction

ISO-TP (ISO 15765-2) is a transport protocol that enables reliable transmission of large data payloads over CAN/TWAI networks. It automatically segments messages larger than 7 bytes and provides flow control to prevent buffer overflow.

**When to use ISO-TP:**
- Diagnostic applications (UDS, OBD-II)
- Firmware updates over CAN
- Large data transfers between ECUs
- Applications requiring message acknowledgment

**Key Features:**
- Automatic segmentation/reassembly for messages up to 4095 bytes
- Built-in flow control and error detection
- Non-blocking API suitable for real-time applications
- Multi-instance support for different CAN IDs

> **Note**: This implementation currently supports classic CAN frames only. TWAI-FD (CAN-FD) is not supported yet in this version.

## Basic Usage

### Simple Echo Server

Here's a complete example showing how to create an ISO-TP echo server:

```c
#include "esp_isotp.h"
#include "esp_twai_onchip.h"

void app_main(void)
{
    // Step 1: Initialize TWAI node
    twai_onchip_node_config_t twai_cfg = {
        .io_cfg = {
            .tx = GPIO_NUM_5,     // CAN TX pin
            .rx = GPIO_NUM_4,     // CAN RX pin
        },
        .bit_timing = {
            .bitrate = 500000,    // 500 kbit/s
        },
        .tx_queue_depth = 16,
    };
    
    twai_node_handle_t twai_node;
    ESP_ERROR_CHECK(twai_new_node_onchip(&twai_cfg, &twai_node));

    // Step 2: Configure ISO-TP transport
    esp_isotp_config_t isotp_cfg = {
        .tx_id = 0x7E0,           // Send to this CAN ID
        .rx_id = 0x7E8,           // Receive from this CAN ID
        .tx_buffer_size = 4096,   // Max outgoing message size
        .rx_buffer_size = 4096,   // Max incoming message size
    };

    // Step 3: Create ISO-TP transport
    esp_isotp_handle_t isotp_handle;
    ESP_ERROR_CHECK(esp_isotp_new_transport(twai_node, &isotp_cfg, &isotp_handle));

    // Step 4: Main communication loop
    uint8_t buffer[4096];
    uint32_t received_size;
    
    while (1) {
        // CRITICAL: Poll every 1-10ms for proper operation
        esp_isotp_poll(isotp_handle);
        
        // Check for incoming messages
        if (esp_isotp_receive(isotp_handle, buffer, sizeof(buffer), &received_size) == ESP_OK) {
            printf("Received %ld bytes: ", received_size);
            for (int i = 0; i < received_size; i++) {
                printf("%02X ", buffer[i]);
            }
            printf("\\n");
            
            // Echo back the received data
            esp_isotp_send(isotp_handle, buffer, received_size);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### Point-to-Point Communication

For applications that need to send specific messages:

```c
void send_diagnostic_request(esp_isotp_handle_t handle)
{
    // UDS Read Data Identifier request
    uint8_t request[] = {0x22, 0xF1, 0x90};  // Service 0x22, DID 0xF190
    
    esp_err_t ret = esp_isotp_send(handle, request, sizeof(request));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Diagnostic request sent");
    } else if (ret == ESP_ERR_NOT_FINISHED) {
        ESP_LOGW(TAG, "Previous transmission still in progress");
    } else {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(ret));
    }
}

void receive_response(esp_isotp_handle_t handle)
{
    uint8_t response[256];
    uint32_t size;
    
    esp_err_t ret = esp_isotp_receive(handle, response, sizeof(response), &size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Received %ld byte response", size);
        // Process the response...
    } else if (ret == ESP_ERR_NOT_FOUND) {
        // No complete message available yet - this is normal
    }
}
```

## Configuration

### Basic Configuration

The [`esp_isotp_config_t`](api.md#struct-esp_isotp_config_t) structure controls the transport behavior:

```c
esp_isotp_config_t config = {
    .tx_id = 0x7E0,           // CAN ID for outgoing messages
    .rx_id = 0x7E8,           // CAN ID for incoming messages
    .tx_buffer_size = 4096,   // Maximum message size to send
    .rx_buffer_size = 4096,   // Maximum message size to receive
};
```

**Configuration Guidelines:**

| Use Case | TX Buffer | RX Buffer | Description |
|----------|-----------|-----------|-------------|
| **Diagnostics** | 256 bytes | 4096 bytes | Small requests, large responses |
| **Firmware Update** | 4096 bytes | 256 bytes | Large data uploads, small confirmations |
| **General Purpose** | 1024 bytes | 1024 bytes | Balanced bidirectional communication |

### Advanced Configuration

Protocol timing can be adjusted in `isotp_config.h`:

```c
#define ISO_TP_DEFAULT_BLOCK_SIZE       8     // Frames per flow control
#define ISO_TP_DEFAULT_ST_MIN_US        0     // Minimum frame separation (μs)
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 100000  // 100ms timeout
```

**Timing Recommendations:**

- **High-speed networks (500K-1M bps)**: Block size 16-32, STmin 0-2ms
- **Standard networks (125K-250K bps)**: Block size 8, STmin 5-10ms
- **Noisy environments**: Block size 4, STmin 10-20ms, longer timeouts

## Event Callbacks

ISO-TP supports callbacks for transmission and reception completion:

```c
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
void on_tx_done(void *user_arg, uint32_t tx_size)
{
    ESP_LOGI(TAG, "Transmission complete: %ld bytes sent", tx_size);
}

// Register callback
esp_isotp_set_tx_done_callback(isotp_handle, on_tx_done, NULL);
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
void on_rx_done(void *user_arg, const uint8_t *data, uint32_t size)
{
    ESP_LOGI(TAG, "Reception complete: %ld bytes received", size);
    // Note: Data is only valid during this callback
}

// Register callback
esp_isotp_set_rx_done_callback(isotp_handle, on_rx_done, NULL);
#endif
```

## Multiple Instances

You can create multiple ISO-TP instances for different communication channels:

```c
void setup_multiple_channels(twai_node_handle_t twai_node)
{
    // Diagnostic channel
    esp_isotp_config_t diag_cfg = {
        .tx_id = 0x7E0, .rx_id = 0x7E8,
        .tx_buffer_size = 256, .rx_buffer_size = 4096,
    };
    esp_isotp_handle_t diag_handle;
    ESP_ERROR_CHECK(esp_isotp_new_transport(twai_node, &diag_cfg, &diag_handle));

    // Data transfer channel
    esp_isotp_config_t data_cfg = {
        .tx_id = 0x7E1, .rx_id = 0x7E9,
        .tx_buffer_size = 4096, .rx_buffer_size = 4096,
    };
    esp_isotp_handle_t data_handle;
    ESP_ERROR_CHECK(esp_isotp_new_transport(twai_node, &data_cfg, &data_handle));

    // Handle both channels in the same loop
    while (1) {
        esp_isotp_poll(diag_handle);
        esp_isotp_poll(data_handle);
        // ... handle messages for both channels
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

## Error Handling

Proper error handling ensures robust operation:

```c
esp_err_t send_with_retry(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size)
{
    esp_err_t ret;
    int retry_count = 0;
    const int max_retries = 3;
    
    do {
        ret = esp_isotp_send(handle, data, size);
        
        switch (ret) {
            case ESP_OK:
                ESP_LOGI(TAG, "Message sent successfully");
                return ESP_OK;
                
            case ESP_ERR_NOT_FINISHED:
                ESP_LOGW(TAG, "Previous transmission in progress, waiting...");
                vTaskDelay(pdMS_TO_TICKS(10));
                retry_count++;
                break;
                
            case ESP_ERR_NO_MEM:
                ESP_LOGE(TAG, "Message too large for buffer (%ld bytes)", size);
                return ret;
                
            default:
                ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(ret));
                return ret;
        }
    } while (retry_count < max_retries);
    
    ESP_LOGE(TAG, "Send failed after %d retries", max_retries);
    return ESP_ERR_TIMEOUT;
}
```

## Performance Optimization

### High-Throughput Applications

```c
void configure_for_speed(void)
{
    // In isotp_config.h, use these settings:
    // #define ISO_TP_DEFAULT_BLOCK_SIZE 32
    // #define ISO_TP_DEFAULT_ST_MIN_US 0
    
    // Poll more frequently
    while (1) {
        esp_isotp_poll(isotp_handle);
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms polling
    }
}
```

### Memory-Constrained Systems

```c
void configure_for_memory(void)
{
    esp_isotp_config_t config = {
        .tx_id = 0x7E0, .rx_id = 0x7E8,
        .tx_buffer_size = 256,   // Smaller buffers
        .rx_buffer_size = 256,
    };
    
    // In isotp_config.h:
    // #define ISO_TP_DEFAULT_BLOCK_SIZE 4  // Smaller blocks
    // #define ISO_TP_DEFAULT_ST_MIN_US 5000  // 5ms separation
}
```

## FAQ

### Q: Why does my multi-frame transmission fail?

A: The most common cause is insufficient polling frequency. [`esp_isotp_poll()`](api.md#function-esp_isotp_poll) must be called every 1-10ms to handle timeouts and send consecutive frames.

```c
// Wrong - polling too slow
while (1) {
    esp_isotp_poll(handle);
    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms - TOO SLOW!
}

// Correct - proper polling frequency
while (1) {
    esp_isotp_poll(handle);
    vTaskDelay(pdMS_TO_TICKS(5));    // 5ms - Good
}
```

### Q: Can I send multiple messages simultaneously?

A: No, each ISO-TP instance can handle only one transmission at a time. Use multiple instances with different CAN IDs for parallel communication.

```c
// Wrong - will return ESP_ERR_NOT_FINISHED
esp_isotp_send(handle, data1, size1);
esp_isotp_send(handle, data2, size2);  // This will fail

// Correct - use separate instances
esp_isotp_send(handle1, data1, size1);
esp_isotp_send(handle2, data2, size2);
```

### Q: How do I know when a transmission is complete?

A: Use callbacks for precise timing, or check the send status:

```c
// Method 1: Use callback (recommended)
esp_isotp_set_tx_done_callback(handle, on_tx_complete, NULL);

// Method 2: Check send status
esp_err_t ret = esp_isotp_send(handle, data, size);
if (ret == ESP_OK) {
    // Transmission started, continue polling until complete
    while (is_transmission_in_progress()) {
        esp_isotp_poll(handle);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

### Q: What's the maximum message size?

A: ISO-TP supports up to 4095 bytes per message, but this is limited by your configured buffer sizes in [`esp_isotp_config_t`](api.md#struct-esp_isotp_config_t).

### Q: Why do I get timeouts on slow networks?

A: Increase the timeout value in `isotp_config.h`:

```c
// For slow networks (125 kbps), use longer timeouts
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 500000  // 500ms instead of 100ms
```

## Protocol Details

For in-depth understanding of the ISO-TP protocol including frame formats, timing parameters, and message flow examples, see the [Protocol Details](protocol.md).

## API Reference

For detailed API documentation including function signatures, parameters, and return values, see the [API Reference](api.md).