# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Sign a manifest and assemble an envelope. The caller decides every byte of the manifest."""
from __future__ import annotations

import hashlib
from dataclasses import dataclass

from . import codes, cose
from .cbor import Tag, dumps, head


def digest(data):
    """SUIT_Digest of raw bytes."""
    return [codes.SHA256, hashlib.sha256(bytes(data)).digest()]


def bstr_digest(content):
    """SUIT_Digest of a byte-string member: the digest covers the bstr head and its content."""
    return digest(dumps(bytes(content)))


@dataclass
class Signer:
    key: object                       # cryptography EllipticCurvePrivateKey, P-256
    alg: int = codes.ESP256
    kid: bytes | None = None


def authentication(manifest_bytes, signers, digest_override=None):
    """SUIT_Authentication: the manifest digest, then one COSE_Sign1 per signer."""
    d = dumps(digest_override if digest_override is not None else bstr_digest(manifest_bytes))
    return dumps([d] + [dumps(cose.sign1(d, s.key, s.alg, s.kid)) for s in signers])


def envelope(auth, manifest_bytes, severed=None, integrated=None, extra=None, tagged=True):
    """The envelope map. severed maps 16/20/23 to section bytes, integrated maps URI to payload."""
    env = {codes.ENV_AUTH: auth, codes.ENV_MANIFEST: manifest_bytes}
    for part in (severed, integrated, extra):
        env.update(part or {})
    return dumps(Tag(codes.ENVELOPE_TAG, env) if tagged else env)


def _read_head(data, pos):
    ib = data[pos]
    ai = ib & 0x1f
    if ai < 24:
        return ib >> 5, ai, pos + 1
    size = {24: 1, 25: 2, 26: 4, 27: 8}[ai]
    return ib >> 5, int.from_bytes(data[pos + 1:pos + 1 + size], 'big'), pos + 1 + size


def entry_offsets(env_bytes):
    """Byte position of every envelope member and of its content, for transports that fetch ranges."""
    pos = 0
    major, value, pos = _read_head(env_bytes, pos)
    if major == 6:
        major, value, pos = _read_head(env_bytes, pos)
    out = {}
    for _ in range(value):
        kmajor, kval, after = _read_head(env_bytes, pos)
        if kmajor == 3:
            key = env_bytes[after:after + kval].decode()
            after += kval
        else:
            key = kval if kmajor == 0 else -1 - kval
        _, vlen, content = _read_head(env_bytes, after)
        out[str(key)] = {'offset': after, 'size': content - after + vlen,
                         'content_offset': content, 'content_size': vlen}
        pos = content + vlen
    return out


__all__ = ['digest', 'bstr_digest', 'Signer', 'authentication', 'envelope', 'entry_offsets', 'head']
