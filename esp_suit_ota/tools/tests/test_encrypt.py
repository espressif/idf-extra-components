# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Encrypted payloads: the wire format, the round trip, and the refusals."""
import pytest
from cryptography.hazmat.primitives.asymmetric import ec

from esp_suit_tool import codes, keys, release
from esp_suit_tool.cbor import Tag, loads
from esp_suit_tool.encrypt import EncryptError, decrypt_payload, encrypt_payload
from esp_suit_tool.verify import verify

# draft-ietf-suit-firmware-encryption section 6.2.1
VECTOR_KEY = bytes.fromhex('261DE6165070FB8951EC5D7B92A065FE')
VECTOR_IV = bytes.fromhex('DAE613B2E0DC55F4322BE38BDBA9DC68')
VECTOR_PLAIN = b'This is a real firmware image.'
VECTOR_CIPHER = bytes.fromhex('2BB8DB522AE978246CC775C3B0241BD4B0333FFDD2DB70C7EE7A4966E3B7')

PLAIN = bytes(range(256)) * 8


@pytest.fixture
def device():
    return keys.generate()


def test_aes_ctr_matches_the_draft_test_vector(device):
    out = encrypt_payload(VECTOR_PLAIN, device.public_key(), codes.A128CTR,
                          cek=VECTOR_KEY, iv=VECTOR_IV)
    assert out.ciphertext == VECTOR_CIPHER


def test_encryption_info_shape(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM)
    top = loads(out.info)
    assert isinstance(top, Tag) and top.tag == codes.COSE_ENCRYPT_TAG
    protected, unprotected, ciphertext, recipients = top.value
    assert loads(protected)[codes.HDR_ALG] == codes.A128GCM
    assert len(unprotected[codes.HDR_IV]) == 12
    assert ciphertext is None, 'the ciphertext is detached'
    assert len(recipients) == 1
    rp, ru, wrapped = recipients[0]
    assert loads(rp)[codes.HDR_ALG] == codes.ECDH_ES_A128KW
    ek = ru[codes.HDR_EPHEMERAL_KEY]
    assert ek[codes.KEY_KTY] == codes.KTY_EC2 and ek[codes.KEY_CRV] == codes.CRV_P256
    assert len(ek[codes.KEY_X]) == 32 and len(ek[codes.KEY_Y]) == 32
    assert len(wrapped) == 24, 'a wrapped 16-byte key is 24 bytes'


def test_aes_ctr_protected_header_is_zero_length(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128CTR)
    protected, unprotected, _, _ = loads(out.info).value
    assert protected == b'', 'draft section 6 requires it'
    assert unprotected[codes.HDR_ALG] == codes.A128CTR
    assert len(unprotected[codes.HDR_IV]) == 16


@pytest.mark.parametrize('alg', [codes.A128GCM, codes.A256GCM, codes.A128CTR, codes.A256CTR])
def test_round_trip(device, alg):
    out = encrypt_payload(PLAIN, device.public_key(), alg)
    assert out.ciphertext != PLAIN
    assert decrypt_payload(out.ciphertext, out.info, device) == PLAIN


@pytest.mark.parametrize('alg', [codes.A128GCM, codes.A128CTR])
def test_every_release_uses_a_fresh_content_key(device, alg):
    a = encrypt_payload(PLAIN, device.public_key(), alg)
    b = encrypt_payload(PLAIN, device.public_key(), alg)
    assert a.cek != b.cek
    assert a.ciphertext != b.ciphertext


def test_an_aead_appends_its_tag(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM)
    assert len(out.ciphertext) == len(PLAIN) + codes.GCM_TAG_BYTES


def test_aes_ctr_keeps_the_length(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128CTR)
    assert len(out.ciphertext) == len(PLAIN)


def test_another_device_cannot_unwrap(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM)
    with pytest.raises(EncryptError, match='another device'):
        decrypt_payload(out.ciphertext, out.info, keys.generate())


def test_a_changed_byte_fails_the_tag(device):
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM)
    bad = bytearray(out.ciphertext)
    bad[0] ^= 0x01
    with pytest.raises(EncryptError, match='tag'):
        decrypt_payload(bytes(bad), out.info, device)


def test_a_fixed_ephemeral_key_is_reproducible(device):
    fixed = ec.derive_private_key(0x0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A,
                                  ec.SECP256R1())
    a = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM, cek=VECTOR_KEY,
                        iv=bytes(12), ephemeral=fixed)
    b = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM, cek=VECTOR_KEY,
                        iv=bytes(12), ephemeral=fixed)
    assert a.info == b.info and a.ciphertext == b.ciphertext


def _release(tmp_path, device, content='A128GCM'):
    sign = keys.generate()
    keys.write_private(sign, str(tmp_path / 'sign.pem'))
    (tmp_path / 'device_pub.bin').write_bytes(keys.anchor(device.public_key()))
    (tmp_path / 'fw.bin').write_bytes(PLAIN)
    spec = {
        'sequence_number': 7,
        'vendor': 'example.com',
        'class': 'my-board-rev2',
        'components': [{'id': ['esp32c5', 'app'], 'file': 'fw.bin', 'uri': '#fw'}],
        'encryption': {'recipient': 'device_pub.bin', 'content': content},
        'signing': {'keys': ['sign.pem']},
    }
    r = release.from_spec(spec, str(tmp_path))
    return sign, release.build(r)


def test_a_release_carries_the_encryption_info(tmp_path, device):
    _, built = _release(tmp_path, device)
    comp = built.info['components'][0]
    assert comp['encrypted'] is True
    assert comp['size'] == len(PLAIN) + codes.GCM_TAG_BYTES, 'the size covers the fetched bytes'
    assert comp['plaintext_size'] == len(PLAIN)


def test_verify_checks_an_encrypted_payload_with_the_device_key(tmp_path, device):
    sign, built = _release(tmp_path, device)
    report = verify(built.envelope, [sign.public_key()], recipient_private=device)
    assert report.ok and not report.failed()
    assert any('after decryption' in c.detail for c in report.checks)


def test_verify_without_the_device_key_says_so(tmp_path, device):
    sign, built = _release(tmp_path, device)
    report = verify(built.envelope, [sign.public_key()])
    assert report.ok
    assert any('--recipient-key' in c.detail for c in report.checks)


def test_verify_refuses_an_encrypted_payload_with_the_wrong_key(tmp_path, device):
    sign, built = _release(tmp_path, device)
    report = verify(built.envelope, [sign.public_key()], recipient_private=keys.generate())
    assert not report.ok


def test_a_release_without_an_encryption_block_stays_in_the_clear(tmp_path, device):
    sign = keys.generate()
    keys.write_private(sign, str(tmp_path / 'sign.pem'))
    (tmp_path / 'fw.bin').write_bytes(PLAIN)
    spec = {
        'sequence_number': 7, 'vendor': 'example.com', 'class': 'my-board-rev2',
        'components': [{'id': ['esp32c5', 'app'], 'file': 'fw.bin', 'uri': '#fw'}],
        'signing': {'keys': ['sign.pem']},
    }
    built = release.build(release.from_spec(spec, str(tmp_path)))
    assert built.info['components'][0]['encrypted'] is False
    assert verify(built.envelope, [sign.public_key()]).ok


def test_an_unknown_content_algorithm_is_refused(tmp_path, device):
    (tmp_path / 'device_pub.bin').write_bytes(keys.anchor(device.public_key()))
    (tmp_path / 'fw.bin').write_bytes(PLAIN)
    sign = keys.generate()
    keys.write_private(sign, str(tmp_path / 'sign.pem'))
    spec = {
        'sequence_number': 7, 'vendor': 'example.com', 'class': 'my-board-rev2',
        'components': [{'id': ['esp32c5', 'app'], 'file': 'fw.bin', 'uri': '#fw'}],
        'encryption': {'recipient': 'device_pub.bin', 'content': 'AES-CBC'},
        'signing': {'keys': ['sign.pem']},
    }
    with pytest.raises(release.ReleaseError, match='encryption.content'):
        release.from_spec(spec, str(tmp_path))


def test_an_encryption_block_without_a_recipient_is_refused(tmp_path):
    with pytest.raises(release.ReleaseError, match='recipient'):
        release.from_spec({'vendor': 'example.com', 'class': 'c',
                           'components': [{'id': ['a'], 'file': 'f', 'uri': 'u'}],
                           'encryption': {'content': 'A128GCM'}}, str(tmp_path))


def test_a_salt_is_carried_in_the_protected_header(device):
    salt = bytes(range(16))
    out = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM, salt=salt)
    _, _, _, recipients = loads(out.info).value
    protected = loads(recipients[0][0])
    assert protected[codes.HDR_SALT] == salt, 'a public salt travels in the protected header'
    assert decrypt_payload(out.ciphertext, out.info, device) == PLAIN


def test_a_salt_changes_the_derived_key(device):
    fixed = ec.derive_private_key(0x0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A1B0E9E3A,
                                  ec.SECP256R1())
    plain = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM, cek=VECTOR_KEY,
                            iv=bytes(12), ephemeral=fixed)
    salted = encrypt_payload(PLAIN, device.public_key(), codes.A128GCM, cek=VECTOR_KEY,
                             iv=bytes(12), ephemeral=fixed, salt=bytes(16))
    assert plain.info != salted.info, 'the same content key wraps differently under a salt'
    assert decrypt_payload(salted.ciphertext, salted.info, device) == PLAIN


def test_a_salted_release_round_trips(tmp_path, device):
    sign = keys.generate()
    keys.write_private(sign, str(tmp_path / 'sign.pem'))
    (tmp_path / 'device_pub.bin').write_bytes(keys.anchor(device.public_key()))
    (tmp_path / 'fw.bin').write_bytes(PLAIN)
    spec = {
        'sequence_number': 7, 'vendor': 'example.com', 'class': 'my-board-rev2',
        'components': [{'id': ['esp32c5', 'app'], 'file': 'fw.bin', 'uri': '#fw'}],
        'encryption': {'recipient': 'device_pub.bin', 'content': 'A128GCM', 'salt': 16},
        'signing': {'keys': ['sign.pem']},
    }
    built = release.build(release.from_spec(spec, str(tmp_path)))
    report = verify(built.envelope, [sign.public_key()], recipient_private=device)
    assert report.ok and not report.failed()


def test_an_oversized_salt_is_refused(tmp_path, device):
    (tmp_path / 'device_pub.bin').write_bytes(keys.anchor(device.public_key()))
    with pytest.raises(release.ReleaseError, match='encryption.salt'):
        release.from_spec({'vendor': 'example.com', 'class': 'c',
                           'components': [{'id': ['a'], 'file': 'f', 'uri': 'u'}],
                           'encryption': {'recipient': 'device_pub.bin', 'salt': 999}},
                          str(tmp_path))
