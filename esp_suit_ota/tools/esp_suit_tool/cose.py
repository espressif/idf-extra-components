# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""COSE_Sign1 with a detached payload, the way SUIT authenticates a manifest.

Draft-ietf-suit-manifest-38 section 8.3: the detached payload bytes are the contents of the first
byte string of SUIT_Authentication, the CBOR encoding of the SUIT_Digest array. They become the
payload field of Sig_structure. The payload field of the transmitted COSE_Sign1 is nil.
"""
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature, encode_dss_signature

from . import codes
from .cbor import Tag, dumps, loads


class CoseError(Exception):
    pass


def sig_structure(protected, payload):
    return dumps(['Signature1', protected, b'', payload])


def sign1(payload, key, alg=codes.ESP256, kid=None, deterministic=True):
    """A tagged COSE_Sign1 over payload. Deterministic ECDSA (RFC 6979) by default."""
    if alg not in codes.SIGNATURE_ALGS.values():
        raise CoseError(f'algorithm {alg} is not supported, use ESP256 (-9) or ES256 (-7)')
    header = {1: alg}
    if kid is not None:
        header[4] = kid.encode() if isinstance(kid, str) else bytes(kid)
    protected = dumps(header)
    der = key.sign(sig_structure(protected, payload), ec.ECDSA(hashes.SHA256(), deterministic_signing=deterministic))
    r, s = decode_dss_signature(der)
    return Tag(codes.COSE_SIGN1_TAG, [protected, {}, None, r.to_bytes(32, 'big') + s.to_bytes(32, 'big')])


def verify1(block, payload, public_keys):
    """Index of the first key that verifies a COSE_Sign1 block; CoseError otherwise."""
    if not isinstance(block, Tag) or block.tag != codes.COSE_SIGN1_TAG:
        raise CoseError('the block is not a tagged COSE_Sign1')
    if not isinstance(block.value, list) or len(block.value) != 4:
        raise CoseError('COSE_Sign1 is not an array of four')
    protected, _unprotected, embedded, sig = block.value
    if embedded is not None:
        raise CoseError('the payload field must be nil: SUIT signs a detached payload')
    header = loads(protected) if protected else {}
    alg = header.get(1)
    if alg not in codes.SIGNATURE_ALGS.values():
        raise CoseError(f'algorithm {alg} is not ESP256 or ES256')
    if not isinstance(sig, bytes) or len(sig) != 64:
        raise CoseError('an ECDSA P-256 signature is 64 bytes')
    der = encode_dss_signature(int.from_bytes(sig[:32], 'big'), int.from_bytes(sig[32:], 'big'))
    tbs = sig_structure(protected, payload)
    for i, pub in enumerate(public_keys):
        try:
            pub.verify(der, tbs, ec.ECDSA(hashes.SHA256()))
            return i
        except InvalidSignature:
            continue
    raise CoseError('no given key verifies the signature')
