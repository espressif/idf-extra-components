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

## YMODEM Framework (Planned)

YMODEM transfers a set of files, not a single anonymous byte stream. Besides the payload used by XMODEM, it carries file metadata (`ymodem_file_info_t`: name, optional size, modification time, and mode) and explicit file boundaries.

YMODEM is not yet implemented. The following describes the planned design and APIs.

YMODEM uses the same [`xymodem_transport_t`](api.md) as XMODEM. Applications provide a **file source** when sending and a **file sink** when receiving:

```
+----------------+              +----------------+      +----------------+
|  File Source   |              |                |      |   Transport    |
|                |              |                |      |                |
|    next()      | --fileinfo-> |                |      |                |
|    read()      | ----data---> |     YMODEM     | ---> |    send()      |
|    end()       | <--notify--- |                |      |                |
+----------------+              |                |      |                |
                                |                |      |                |
+----------------+              |                |      |                |
|   File Sink    |              |                |      |                |
|                |              |                |      |                |
|    next()      | <-fileinfo-- |                | <--- |    recv()      |
|    write()     | <---data---- |                |      |                |
|    end()       | <--notify--- |                |      |                |
+----------------+              +----------------+      +----------------+
```

- **File source** (`ymodem_file_source_t`): `next()` returns metadata for the next file to send. Set `filename` to `NULL` or an empty string when there are no more files. `read()` supplies that file's payload. `end()` is called when that file's transfer finishes (success or failure) so the application can close the file or release other resources. The `filename` pointer from `next()` must remain valid until the matching `end()`.
- **File sink** (`ymodem_file_sink_t`): `next()` delivers metadata for the incoming file (the `filename` pointer is valid only during that callback). Use it to create or open the destination. `write()` receives the payload. `end()` notifies completion so the application can close the file.

A file source or sink does not have to be backed by a filesystem. Any object that can enumerate items, supply or consume a byte stream, and clean up on `end()` is valid.

A typical call flow:

```
ymodem_send()
|  next() -> get file1
|  read() -> read file1
|  read() -> read file1
|  end()  -> close file1
|
|  next() -> get file2
|  read() -> read file2
|  end()  -> close file2
|
|  next() -> no more files, transfer complete

ymodem_recv()
|  next() -> accept file1
|  write() -> write file1
|  write() -> write file1
|  end()  -> close file1
|
|  next() -> accept file2
|  write() -> write file2
|  end()  -> close file2
```

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
