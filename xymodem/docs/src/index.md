# X/YMODEM Programming Guide

The `xymodem` component implements the XMODEM and YMODEM protocol layer. It handles handshaking, packet framing, checksum or CRC verification, and retries. Applications provide a **transport** for the physical link and a **source** or **sink** for the payload.

Currently supported features:

- XMODEM
- XMODEM-CRC (CRC-16)
- XMODEM-1K (1024-byte blocks)

Not supported yet:

- YMODEM
- YMODEM-G

## Add the Component to Your Project

```bash
idf.py add-dependency "espressif/xymodem"
```

## XMODEM Framework

The protocol layer sits between application data and the communication channel. Users provide a **transport** plus a **data source** (send) or **data sink** (receive):

```
+----------------+      +----------------+      +----------------+
|  Data Source   |      |                |      |   Transport    |
|                |      |                |      |                |
|    read()      | ---> |     XMODEM     | ---> |    send()      |
+----------------+      |                |      |                |
                        |                |      |                |
+----------------+      |                |      |                |
|   Data Sink    |      |                |      |                |
|                |      |                |      |                |
|    write()     | <--- |                | <--- |    recv()      |
+----------------+      +----------------+      +----------------+
```

- **Transport** (`xymodem_transport_t`): `send()` and `recv()` over UART, UHCI, or any other byte stream. Timeout on `recv()` is not treated as an error; a short read is only allowed after the timeout expires.
- **Data source** (`xmodem_data_source_t`): where the sender reads payload bytes. The source may be an array, a file, or any other stream.
- **Data sink** (`xmodem_data_sink_t`): where the receiver delivers payload bytes. XMODEM cannot distinguish trailing padding (`0x1A`) from real data; the padding is also passed to `write()`. The sink may store the data or process it on the fly.

All transport and source/sink callbacks are user-provided and run in task context.

## Receiver-Driven Model and Timeouts

The original XMODEM specification recommends a receiver-driven model. After sending a packet, the sender does not retransmit unless it receives an explicit `NAK`. If data is corrupted or lost, retransmission is always initiated by the receiver using a relatively short timeout. The sender uses a much longer timeout; if no valid response arrives, it assumes the link is completely lost. This helps avoid the confusion that can arise when both sides attempt retransmission independently.

This component follows that design. The sender configuration [`response_timeout_ms`](api.md) is the time to wait for the handshake or for `ACK` / `NAK`. It is **not** a retransmission timeout. If it expires with no valid response, the sender treats the link as lost (`ESP_ERR_XYMODEM_NO_RESPONSE`). The receiver configuration [`packet_timeout_ms`](api.md) is the time to wait for a packet before sending `NAK`. It is recommended to set `response_timeout_ms` several times longer than `packet_timeout_ms`.

The [`XYMODEM_SEND_CONFIG_DEFAULT()`](api.md) and [`XYMODEM_RECV_CONFIG_DEFAULT()`](api.md) macros provide a set of configurations suitable for reliable connections and modern peer devices. If different behavior is required, you can override individual settings or provide your own configuration.

## Using XMODEM

Provide a transport, a send or receive configuration, and a source or sink, then call [`xmodem_send()`](api.md) or [`xmodem_recv()`](api.md). Note that both functions block the current task until the transfer completes.

Sender:

```c
#include "xymodem.h"

xymodem_transport_t transport = {
    .recv = my_uart_recv,
    .send = my_uart_send,
    .ctx = uart_ctx,
};

/**
 * Default sender configuration:
 * - Use 1024-byte blocks by default, and switch to 128-byte blocks if the remaining data is no more than 128 bytes.
 * - Wait for handshake or ACK/NAK for 30 seconds.
 * - Maximum 10 extra retries after the first attempt.
 * - Send 2 CAN bytes when aborting.
 */
xymodem_send_config_t send_config = XYMODEM_SEND_CONFIG_DEFAULT();

xmodem_data_source_t source = {
    .read = my_read,
    .ctx = file_ctx,
};

ESP_ERROR_CHECK(xmodem_send(&transport, &send_config, &source));
```

Receiver:

```c
#include "xymodem.h"

xymodem_transport_t transport = {
    .recv = my_uart_recv,
    .send = my_uart_send,
    .ctx = uart_ctx,
};

/**
 * Default receiver configuration:
 * - Use CRC-16 by default, and fall back to SUM if the peer does not support it.
 * - Wait up to 3 seconds for the packet. If none is received, send a NAK to request retransmission.
 * - Maximum 10 extra retries after the first attempt.
 * - Send 2 CAN bytes when aborting.
 */
xymodem_recv_config_t recv_config = XYMODEM_RECV_CONFIG_DEFAULT();

xmodem_data_sink_t sink = {
    .write = my_write,
    .ctx = file_ctx,
};

ESP_ERROR_CHECK(xmodem_recv(&transport, &recv_config, &sink));
```

The `examples/xmodem` project shows a complete transport on the UART driver.

If the payload is already a contiguous buffer, you do not need a source or sink. [`xmodem_send_buffer()`](api.md) sends `len` bytes from `buf` as the XMODEM payload. [`xmodem_recv_buffer()`](api.md) writes the incoming stream into `buf` (at most `len` bytes) and reports how many bytes were stored. Use the generic `read` / `write` callbacks instead when data comes from a file, is generated on the fly, or must be processed as each packet arrives.

XMODEM cannot tell payload from trailing `0x1A` padding, so `xmodem_recv_buffer()` stores that padding as well. If the last packet is larger than the remaining buffer space, the extra bytes in that packet are discarded (they may be padding). If another packet arrives after the buffer is already full, the transfer is aborted with `ESP_ERR_NO_MEM`.

## API Reference

See [API Reference](api.md) for types, error codes, and function details.
