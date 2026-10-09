#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Send a 0, 1, 2, ... pattern, then receive and verify the board echo."""

import argparse
import sys
from io import BytesIO

try:
    import serial
    from xmodem import XMODEM
except ImportError:
    print('Please install dependencies: pip install -r tools/requirements.txt', file=sys.stderr)
    sys.exit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-p', '--port', required=True, help='USB-UART port, e.g. /dev/ttyUSB0')
    parser.add_argument('-b', '--baud', type=int, default=115200)
    parser.add_argument('length', type=int, nargs='?', default=2048, help='bytes to send (default: 2048)')
    args = parser.parse_args()

    payload = bytes(i & 0xFF for i in range(args.length))
    with serial.Serial(args.port, args.baud, timeout=30, write_timeout=30) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        def getc(size, timeout=1):
            ser.timeout = timeout
            data = ser.read(size)
            return data or None

        def putc(data, timeout=1):
            ser.write_timeout = timeout
            return ser.write(data)

        modem = XMODEM(getc, putc, mode='xmodem1k')
        print(f'Sending {len(payload)} bytes on {args.port}', flush=True)
        if not modem.send(BytesIO(payload), retry=10, timeout=30, quiet=0):
            print('Send failed', file=sys.stderr)
            return 1

        print('Receiving echo', flush=True)
        received = BytesIO()
        if modem.recv(received, crc_mode=1, retry=10, timeout=30, quiet=0) is None:
            print('Receive failed', file=sys.stderr)
            return 1

    echoed = received.getvalue().rstrip(b'\x1a')
    if echoed != payload:
        print('Echo mismatch', file=sys.stderr)
        return 1
    print(f'Echo ok ({len(payload)} bytes)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
