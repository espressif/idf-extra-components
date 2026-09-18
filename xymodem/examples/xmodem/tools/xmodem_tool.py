#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Host-side XMODEM send/receive helper for the xmodem example."""

from __future__ import annotations

import argparse
import sys
from io import BytesIO
from pathlib import Path

try:
    import serial
except ImportError:
    print("Please install dependencies: pip install -r tools/requirements.txt", file=sys.stderr)
    sys.exit(1)

try:
    from xmodem import XMODEM
except ImportError:
    print("Please install dependencies: pip install -r tools/requirements.txt", file=sys.stderr)
    sys.exit(1)

PAD = 0x1A


def sequential_bytes(length: int) -> bytes:
    return bytes(i & 0xFF for i in range(length))


def strip_xmodem_padding(data: bytes) -> bytes:
    return data.rstrip(bytes([PAD]))


def open_serial(port: str, baud: int, timeout: float) -> serial.Serial:
    ser = serial.Serial(port=port, baudrate=baud, timeout=timeout, write_timeout=timeout)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


def make_modem(ser: serial.Serial, mode: str) -> XMODEM:
    def getc(size: int, timeout: int = 1):
        ser.timeout = timeout
        data = ser.read(size)
        return data or None

    def putc(data: bytes, timeout: int = 1):
        ser.write_timeout = timeout
        return ser.write(data)

    return XMODEM(getc, putc, mode=mode)


def cmd_send(args: argparse.Namespace) -> int:
    if args.pattern is not None:
        payload = sequential_bytes(args.pattern)
        name = f"pattern({args.pattern})"
    else:
        payload = Path(args.file).read_bytes()
        name = args.file

    print(f"Sending {len(payload)} bytes from {name} on {args.port} @ {args.baud} ({args.mode})")

    with open_serial(args.port, args.baud, args.timeout) as ser:
        modem = make_modem(ser, args.mode)
        ok = modem.send(BytesIO(payload), retry=args.retry, timeout=args.timeout, quiet=0)
    if not ok:
        print("Send failed", file=sys.stderr)
        return 1
    print("Send finished")
    return 0


def cmd_recv(args: argparse.Namespace) -> int:
    stream = BytesIO()
    print(f"Receiving on {args.port} @ {args.baud} ({args.mode})")

    with open_serial(args.port, args.baud, args.timeout) as ser:
        modem = make_modem(ser, args.mode)
        n = modem.recv(stream, crc_mode=1, retry=args.retry, timeout=args.timeout, quiet=0)

    if not n:
        print("Receive failed", file=sys.stderr)
        return 1

    data = stream.getvalue()
    if args.file:
        Path(args.file).write_bytes(data)
        print(f"Wrote {len(data)} bytes to {args.file} (includes XMODEM padding)")

    payload = strip_xmodem_padding(data)
    print(f"Received {len(data)} bytes, {len(payload)} bytes after stripping 0x1A padding")

    if args.verify_pattern:
        expected = sequential_bytes(len(payload))
        if payload != expected:
            print("Pattern check failed: payload is not 0, 1, 2, ...", file=sys.stderr)
            return 1
        print(f"Pattern check passed ({len(payload)} bytes)")

    if args.hex:
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            print(" ".join(f"{b:02X}" for b in chunk))

    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="XMODEM host tool for the xymodem example")
    parser.add_argument("--port", "-p", required=True, help="Serial port, e.g. /dev/ttyUSB1 or COM3")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument(
        "--mode",
        choices=("xmodem", "xmodem1k"),
        default="xmodem1k",
        help="Block size: 128-byte XMODEM or 1K (default: xmodem1k)",
    )
    parser.add_argument("--timeout", type=int, default=10, help="I/O timeout in seconds (default: 10)")
    parser.add_argument("--retry", type=int, default=16, help="Protocol retry count (default: 16)")

    sub = parser.add_subparsers(dest="command", required=True)

    send = sub.add_parser("send", help="Send a file or a sequential test pattern")
    src = send.add_mutually_exclusive_group(required=True)
    src.add_argument("--file", "-f", help="File to send")
    src.add_argument(
        "--pattern",
        type=int,
        metavar="LEN",
        help="Send LEN bytes of 0, 1, 2, ... (same as the firmware example)",
    )
    send.set_defaults(func=cmd_send)

    recv = sub.add_parser("recv", help="Receive into a file")
    recv.add_argument("--file", "-f", help="Output file (optional)")
    recv.add_argument("--hex", action="store_true", help="Dump received bytes as hex")
    recv.add_argument(
        "--verify-pattern",
        action="store_true",
        help="Check that the payload (padding stripped) is 0, 1, 2, ...",
    )
    recv.set_defaults(func=cmd_recv)

    return parser


def main() -> int:
    args = build_parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
