# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""P-256 signing keys and the 65-byte trust anchor the device embeds."""
import os

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec


class KeyFileError(Exception):
    pass


def generate():
    return ec.generate_private_key(ec.SECP256R1())


def write_private(key, path):
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption())
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'wb') as f:
        f.write(pem)


def _check(key, path):
    if not isinstance(key.curve, ec.SECP256R1):
        raise KeyFileError(f'{path}: the key is not EC P-256')
    return key


def load_private(path):
    data = open(path, 'rb').read()
    try:
        key = serialization.load_pem_private_key(data, password=None)
    except (ValueError, TypeError) as e:
        raise KeyFileError(f'{path}: not an unencrypted PEM private key ({e})') from None
    if not isinstance(key, ec.EllipticCurvePrivateKey):
        raise KeyFileError(f'{path}: the manifest key must be EC P-256, not {type(key).__name__}')
    return _check(key, path)


def load_public(path):
    """A public key from a PEM private key, a PEM public key, or a 65-byte anchor file."""
    data = open(path, 'rb').read()
    if len(data) == 65 and data[0] == 0x04:
        return ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), data)
    try:
        key = serialization.load_pem_private_key(data, password=None).public_key()
    except (ValueError, TypeError):
        try:
            key = serialization.load_pem_public_key(data)
        except ValueError:
            raise KeyFileError(f'{path}: not a PEM key or a 65-byte P-256 point') from None
    if not isinstance(key, ec.EllipticCurvePublicKey):
        raise KeyFileError(f'{path}: the key is not an EC key')
    return _check(key, path)


def anchor(public_key):
    """The uncompressed point 0x04 || X || Y the device takes as a trust anchor."""
    return public_key.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)


def c_array(name, data, per_line=13):
    rows = [', '.join(f'0x{b:02x}' for b in data[i:i + per_line]) for i in range(0, len(data), per_line)]
    return f'static const uint8_t {name}[{len(data)}] = {{\n' + ''.join(f'    {r},\n' for r in rows) + '};\n'
