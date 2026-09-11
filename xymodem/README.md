# X/YMODEM Protocol Layer

[![Component Registry](https://components.espressif.com/components/espressif/xymodem/badge.svg)](https://components.espressif.com/components/espressif/xymodem)

This directory contains an implementation of the X/YMODEM protocol layer. Currently supported features include:

- XMODEM
- XMODEM-CRC (CRC-16 verification)
- XMODEM-1K (1K-byte blocks)
- YMODEM

The following feature is currently not supported:

- YMODEM-G

## Data Flow Overview

The X/YMODEM protocol layer in this component handles details such as handshaking, packet processing, checksum verification, and retries. The data and file streams above the protocol layer are abstracted as source and sink structures, while the underlying data transfer mechanism is abstracted as a transport structure. Users are required to provide these structures when using the component.

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

A transport is typically implemented on top of UART or UHCI (UART DMA), but other communication mechanisms can also be supported. Users need to provide implementations of the send() and recv() functions in the transport structure. A UART-based transport implementation is available in the example for reference.

When sending data via XMODEM, the user-provided `data source` defines where the protocol layer obtains the data to be transmitted. When receiving data via XMODEM, the user-provided `data sink` defines where the protocol layer writes the received data. A `data source` may be backed by a simple array, a file, or any other object capable of providing a data stream. Similarly, a `data sink` may point to a simple array or a file, or it may process the received data as a stream without storing it at all.

YMODEM is designed to transfer a set of files. In addition to the raw data stream provided by XMODEM, it also needs to handle file metadata and explicitly identify file boundaries. The YMODEM implementation in this component uses the same transport interface as XMODEM, but requires the user to provide more complex file source and sink abstractions.

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

In a `file source`, next() is used by the protocol layer to request metadata for the next file to be sent. In a `file sink`, next() is used by the protocol layer to notify the user of the metadata for the next file being received. The protocol layer then reads or writes the file data through read() or write(). Finally, regardless of whether the transfer succeeds or fails, the protocol layer calls end() to notify the user to perform any necessary cleanup, such as closing the file.

A typical call flow may look like this:

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
|  next() -> get file1
|  write() -> write file1
|  write() -> write file1
|  end()  -> close file1
|
|  next() -> get file2
|  write() -> write file2
|  end()  -> close file2
```

A file source or sink does not have to be backed by actual files. It can be implemented on top of any suitable data provider or consumer.

As a reminder, all interfaces in transport and source/sink are user-provided callback functions and are executed in task context. Typical implementations can be found in the examples. For straightforward data send/receive use cases, the `x/ymodem_send/recv_buffer()` convenience APIs provide a simpler alternative.
