# X/YMODEM Protocol Layer

[![Component Registry](https://components.espressif.com/components/espressif/xymodem/badge.svg)](https://components.espressif.com/components/espressif/xymodem)

XMODEM and YMODEM are classic serial file-transfer protocols. This component implements the protocol layer: handshaking, packet framing, checksum or CRC verification, and retries. Applications provide a transport for the physical link and a source or sink for the payload.

## Features

Currently supported:

- XMODEM
- XMODEM-CRC (CRC-16 verification)
- XMODEM-1K (1K-byte blocks)

Not supported yet:

- YMODEM
- YMODEM-G

## Add to Your Project

Add the `xymodem` component to your project via the ESP Component Registry:

```bash
idf.py add-dependency "espressif/xymodem"
```

## Appendix

- [Xmodem specification](https://www.menie.org/georges/embedded/xmodem_specification.html)
- [XMODEM/YMODEM Protocol Reference](https://techheap.packetizer.com/communications/modems/xmodem-ymodem_reference.html)
