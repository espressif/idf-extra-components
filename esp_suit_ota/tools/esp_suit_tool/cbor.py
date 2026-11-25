# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Deterministic CBOR for SUIT (RFC 8949 section 4.2.1).

The encoder covers the types SUIT uses: integers, byte and text strings, arrays, maps, tags,
booleans and null. Every head takes its shortest form, map keys are sorted bytewise by their
encoding, and a duplicate key is an error. Floats and indefinite lengths cannot be produced.

cbor2 is not used for encoding: its canonical mode sorts keys length-first (RFC 7049), which
differs from RFC 8949 for some key sets.
"""
from collections.abc import Mapping

import cbor2

Tag = cbor2.CBORTag
DecodeError = cbor2.CBORDecodeError


class Raw(bytes):
    """Bytes that are already CBOR. They are copied verbatim, so a test vector can break a rule."""


def head(major, value):
    """The shortest head for a major type and an argument."""
    if value < 0:
        raise ValueError('a CBOR argument is never negative')
    if value < 24:
        return bytes([major << 5 | value])
    for ai, size in ((24, 1), (25, 2), (26, 4), (27, 8)):
        if value < 1 << (8 * size):
            return bytes([major << 5 | ai]) + value.to_bytes(size, 'big')
    raise ValueError('integer does not fit 64 bits')


def dumps(obj):
    """Encode obj in RFC 8949 core deterministic form."""
    if isinstance(obj, Raw):
        return bytes(obj)
    if obj is False:
        return b'\xf4'
    if obj is True:
        return b'\xf5'
    if obj is None:
        return b'\xf6'
    if isinstance(obj, int):
        return head(0, obj) if obj >= 0 else head(1, -1 - obj)
    if isinstance(obj, (bytes, bytearray)):
        return head(2, len(obj)) + bytes(obj)
    if isinstance(obj, str):
        b = obj.encode('utf-8')
        return head(3, len(b)) + b
    if isinstance(obj, (list, tuple)):
        return head(4, len(obj)) + b''.join(dumps(x) for x in obj)
    if isinstance(obj, Mapping):
        items = sorted((dumps(k), dumps(v)) for k, v in obj.items())
        for a, b in zip(items, items[1:]):
            if a[0] == b[0]:
                raise ValueError('duplicate map key')
        return head(5, len(items)) + b''.join(k + v for k, v in items)
    if isinstance(obj, Tag):
        return head(6, obj.tag) + dumps(obj.value)
    raise TypeError(f'{type(obj).__name__} is not part of the SUIT CBOR profile')


def _thaw(obj):
    """cbor2 6 decodes the contents of a tag as immutable types: give back plain dicts and lists."""
    if isinstance(obj, Tag):
        return Tag(obj.tag, _thaw(obj.value))
    if isinstance(obj, Mapping):
        return {k: _thaw(v) for k, v in obj.items()}       # keys stay hashable
    if isinstance(obj, (list, tuple)):
        return [_thaw(x) for x in obj]
    return obj


def loads(data):
    """Decode one CBOR item. Use is_deterministic() to also check the encoding."""
    return _thaw(cbor2.loads(bytes(data)))


def is_deterministic(data):
    """True when data is one item in exactly the form dumps() would produce."""
    try:
        return dumps(loads(data)) == bytes(data)
    except (TypeError, ValueError, DecodeError):
        return False
