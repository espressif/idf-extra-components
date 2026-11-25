# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Encrypted payloads, draft-ietf-suit-firmware-encryption.

The manifest carries suit-parameter-encryption-info (19), a bstr holding SUIT_Encryption_Info,
which is #6.96(COSE_Encrypt) with a detached ciphertext. The payload itself travels as the
component's resource, exactly as a plaintext payload does.

Two profiles are produced here, the signature half of which this component already implements:

  suit-sha256-es256-ecdh-a128ctr   content AES-128-CTR, no tag, integrity from the image digest
  suit-sha256-es256-ecdh-a128gcm   content AES-128-GCM, 16-byte tag appended to the ciphertext

The content encryption key is fresh for every release. It is wrapped for the device with
ECDH-ES + AES-KW: an ephemeral P-256 key agrees a secret with the device's static public key,
HKDF-SHA256 turns that into a key encryption key, and AES-KW wraps the content key with it.

The image digest covers the PLAINTEXT. That is what the device writes into the slot, so it is
also what suit-condition-image-match can read back and hash afterwards. The image size covers
the fetched resource, so for AES-GCM it includes the 16-byte tag.
"""
from __future__ import annotations

import os
from dataclasses import dataclass

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.keywrap import InvalidUnwrap, aes_key_unwrap, aes_key_wrap

from . import codes, keys
from .cbor import Tag, dumps, loads


class EncryptError(Exception):
    pass


@dataclass
class Encrypted:
    ciphertext: bytes            # what the device fetches, tag included for an AEAD
    info: bytes                  # SUIT_Encryption_Info, the value of parameter 19
    cek: bytes                   # kept for tests and for the release summary, never published


def _header_salt(protected, unprotected):
    """The "salt" header parameter, RFC 9053 label -20, from either header bucket."""
    for head in (protected, unprotected):
        if head and codes.HDR_SALT in head:
            return head[codes.HDR_SALT]
    return None


def _kdf_context(kw_alg, protected):
    """COSE_KDF_Context of RFC 9053 section 5.2, with the values draft section 5.2.4 fixes.

    AlgorithmID is the key wrap algorithm, keyDataLength matches it, protected is the serialized
    recipient header, and SuppPubInfo.other is the constant string. Both party identities are
    left empty, which the draft states explicitly.
    """
    key_bits = codes.KEY_BYTES[kw_alg] * 8
    party = [None, None, None]
    return dumps([kw_alg, party, party, [key_bits, protected, codes.KDF_OTHER]])


def _cose_key(point):
    """An EC2 COSE_Key for an uncompressed P-256 point."""
    if len(point) != 65 or point[0] != 0x04:
        raise EncryptError('an ephemeral key must be an uncompressed P-256 point')
    return {codes.KEY_KTY: codes.KTY_EC2, codes.KEY_CRV: codes.CRV_P256,
            codes.KEY_X: point[1:33], codes.KEY_Y: point[33:65]}


def esdh_recipient(recipient_public, cek, kw_alg=codes.A128KW, kid=None, ephemeral=None, salt=None):
    """One COSE_recipient that wraps cek for the holder of recipient_public.

    Returns the recipient array. The ephemeral key is generated here and discarded, so every
    release agrees a different key encryption key even for the same device. A caller may pass
    one in, which only test vectors should do: a fixed ephemeral key makes the output
    reproducible and gives away nothing, because the content key is fresh anyway.
    """
    if kw_alg not in codes.ESDH_FOR_KW:
        raise EncryptError(f'{kw_alg} is not a supported key wrap algorithm')
    ephemeral = ephemeral or ec.generate_private_key(ec.SECP256R1())
    shared = ephemeral.exchange(ec.ECDH(), recipient_public)
    head = {codes.HDR_ALG: codes.ESDH_FOR_KW[kw_alg]}
    if salt is not None:
        head[codes.HDR_SALT] = salt          # public, so it travels in the protected header
    protected = dumps(head)
    kek = HKDF(algorithm=hashes.SHA256(), length=codes.KEY_BYTES[kw_alg], salt=salt,
               info=_kdf_context(kw_alg, protected)).derive(shared)
    unprotected = {codes.HDR_EPHEMERAL_KEY: _cose_key(keys.anchor(ephemeral.public_key()))}
    if kid is not None:
        unprotected[codes.HDR_KID] = kid if isinstance(kid, bytes) else str(kid).encode()
    return [protected, unprotected, aes_key_wrap(kek, cek)]


def _enc_structure(protected):
    """Enc_structure of RFC 9052 section 5.3. The draft fixes external_aad to zero length."""
    return dumps(['Encrypt', protected, b''])


def encrypt_payload(data, recipient_public, content_alg=codes.A128GCM, kw_alg=codes.A128KW,
                    kid=None, iv=None, cek=None, ephemeral=None, salt=None):
    """Encrypt one payload and build the SUIT_Encryption_Info that unlocks it."""
    if content_alg not in codes.KEY_BYTES:
        raise EncryptError(f'{content_alg} is not a supported content encryption algorithm')
    cek = cek or os.urandom(codes.KEY_BYTES[content_alg])
    iv = iv or os.urandom(codes.IV_BYTES[content_alg])
    if len(cek) != codes.KEY_BYTES[content_alg]:
        raise EncryptError('the content key length does not match the algorithm')
    if len(iv) != codes.IV_BYTES[content_alg]:
        raise EncryptError('the IV length does not match the algorithm')

    if content_alg in codes.AEAD_ALGS:
        protected = dumps({codes.HDR_ALG: content_alg})
        ciphertext = AESGCM(cek).encrypt(iv, data, _enc_structure(protected))
        unprotected = {codes.HDR_IV: iv}
    else:
        # draft section 6: the protected header of an AES-CTR structure is a zero-length bstr
        protected = b''
        enc = Cipher(algorithms.AES(cek), modes.CTR(iv)).encryptor()
        ciphertext = enc.update(data) + enc.finalize()
        unprotected = {codes.HDR_ALG: content_alg, codes.HDR_IV: iv}

    recipients = [esdh_recipient(recipient_public, cek, kw_alg, kid, ephemeral, salt)]
    info = dumps(Tag(codes.COSE_ENCRYPT_TAG, [protected, unprotected, None, recipients]))
    return Encrypted(ciphertext=ciphertext, info=info, cek=cek)


def unwrap_cek(info, recipient_private):
    """The content key and the content parameters of a SUIT_Encryption_Info, for the key holder.

    This is the same work the device does: agree with the ephemeral key, derive the key
    encryption key, unwrap the content key. It exists so that verify can check an encrypted
    release end to end, and so the device code has a reference to match.
    """
    outer = loads(info)
    if not isinstance(outer, Tag) or outer.tag != codes.COSE_ENCRYPT_TAG:
        raise EncryptError('encryption info is not #6.96(COSE_Encrypt)')
    body = outer.value
    if not isinstance(body, list) or len(body) != 4:
        raise EncryptError('COSE_Encrypt takes four members')
    protected, unprotected, ciphertext, recipients = body
    if ciphertext is not None:
        raise EncryptError('the ciphertext of an encrypted payload is detached, so the field is nil')
    head = dict(loads(protected)) if protected else {}
    head.update(unprotected or {})
    content_alg = head.get(codes.HDR_ALG)
    iv = head.get(codes.HDR_IV)
    if content_alg not in codes.KEY_BYTES or not isinstance(iv, bytes):
        raise EncryptError('the content algorithm or the IV is missing')
    if not recipients:
        raise EncryptError('no recipient')

    rp, ru, wrapped = recipients[0]
    rhead_protected = dict(loads(rp)) if rp else {}
    rhead = dict(rhead_protected)
    rhead.update(ru or {})
    esdh_alg = rhead.get(codes.HDR_ALG)
    kw_alg = next((k for k, v in codes.ESDH_FOR_KW.items() if v == esdh_alg), None)
    if kw_alg is None:
        raise EncryptError(f'recipient algorithm {esdh_alg} is not ECDH-ES with AES key wrap')
    ek = rhead.get(codes.HDR_EPHEMERAL_KEY)
    if not isinstance(ek, dict) or ek.get(codes.KEY_CRV) != codes.CRV_P256:
        raise EncryptError('the ephemeral key is missing or is not on P-256')
    point = b'\x04' + ek[codes.KEY_X] + ek[codes.KEY_Y]
    peer = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), point)
    shared = recipient_private.exchange(ec.ECDH(), peer)
    kek = HKDF(algorithm=hashes.SHA256(), length=codes.KEY_BYTES[kw_alg],
               salt=_header_salt(rhead_protected, ru), info=_kdf_context(kw_alg, rp)).derive(shared)
    try:
        cek = aes_key_unwrap(kek, wrapped)
    except InvalidUnwrap:
        raise EncryptError('the content key does not unwrap: this release is for another device') from None
    return cek, content_alg, iv, protected


def decrypt_payload(ciphertext, info, recipient_private):
    """The plaintext of a fetched payload. Raises EncryptError when the key does not fit."""
    cek, content_alg, iv, protected = unwrap_cek(info, recipient_private)
    if content_alg in codes.AEAD_ALGS:
        try:
            return AESGCM(cek).decrypt(iv, ciphertext, _enc_structure(protected))
        except InvalidTag:
            raise EncryptError('the AEAD tag does not match: wrong key, or the payload was changed') from None
    dec = Cipher(algorithms.AES(cek), modes.CTR(iv)).decryptor()
    return dec.update(ciphertext) + dec.finalize()
