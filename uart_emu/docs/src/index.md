# UART EMU

## Allocate UART EMULATOR Object with RMT Backend

## Features

- **RMT-based Implementation**: Utilizes ESP32's RMT peripheral for precise timing control
- **Configurable Baud Rate**: Supports various baud rates
- **Asynchronous Operation**: Non-blocking transmit and receive operations
- **Callback Support**: Event-driven programming with TX/RX completion callbacks
- **DMA Support(need RMT Supports DMA)**: Optional DMA acceleration for improved performance
- **Ping-Pong Buffer**: Efficient handling of continuous data streams

## Current Limitations

- **Data Bits**: Only 8-bit data format supported
- **Parity**: Only no-parity mode supported
- **Stop Bits**: 1 or 2 stop bits supported
- **Flow Control**: No hardware flow control support
- **Error Detection**: Limited error detection capabilities compared to hardware UART

## API Documentation

For detailed API documentation, please refer to the header files:
- [uart_emu.h](./include/uart_emu.h) - Main public API
- [uart_emu_rmt.h](./include/uart_emu_rmt.h) - RMT-specific API

## Usage Example

```c
#include "uart_emu_rmt.h"

// Configuration
uart_emu_config_t uart_config = {
    .tx_io_num = UART_EMU_TX_PIN,
    .rx_io_num = UART_EMU_RX_PIN,
    .baud_rate = UART_EMU_BAUD_RATE,
    .data_bits = UART_EMU_DATA_8_BITS,
    .stop_bits = UART_EMU_STOP_BITS_1,
    .parity = UART_EMU_PARITY_DISABLE,
    .rx_buffer_size = UART_EMU_RX_BUFFER_SIZE,
};
uart_emu_rmt_config_t rmt_config = {
    .tx_trans_queue_depth = UART_EMU_RMT_TX_TRANS_QUEUE_DEPTH,
    .tx_mem_block_symbols = UART_EMU_RMT_MEM_BLOCK_SYMBOLS,
    .rx_mem_block_symbols = UART_EMU_RMT_MEM_BLOCK_SYMBOLS,
    .intr_priority = UART_EMU_RMT_INTR_PRIORITY,
    .flags = {
        .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
};

// Initialize
uart_emu_device_handle_t uart_device;
ESP_ERROR_CHECK(uart_emu_new_from_rmt(&uart_config, &rmt_config, &uart_device));

// Transmit data
const char *data = "Hello, World!";
ESP_ERROR_CHECK(uart_emu_transmit(uart_device, (uint8_t *)data, strlen(data)));
```

## Performance Considerations

- Use DMA when available for better performance
- Increase interrupt priority for time-critical applications
- Consider buffer sizes based on your application's data throughput
- The component uses 8 RMT clock cycles per UART bit for better resolution

---

You can create multiple UART EMU objects with different GPIOs. The backend driver will automatically allocate sufficient RMT channels for you wherever possible. If the RMT channels are not enough, the [uart_emu_new_from_rmt](api.md#function-uart_emu_new_from_rmt) will return an error.
