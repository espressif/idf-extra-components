# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""The six signed examples of draft-ietf-suit-manifest-38 Appendix B, checked with the tool."""
import uuid

import pytest

from esp_suit_tool import cbor, codes, cose, ids, keys, verify
from esp_suit_tool.dump import dump

SPEC_VENDOR = bytes.fromhex('fa6b4a53d5ad5fdfbe9de663e4d41ffe')


@pytest.mark.parametrize('i', range(6))
def test_example_verifies_with_the_published_key(appendix_b, spec_pub, i):
    r = verify(appendix_b[i], [spec_pub])
    assert r.ok, str(r)
    assert r.sequence_number == i
    names = {c.name: c.ok for c in r.checks}
    assert names['signature'] and names['manifest digest'] and names['deterministic CBOR']


@pytest.mark.parametrize('i', range(6))
def test_example_fails_with_another_key(appendix_b, i):
    other = keys.generate().public_key()
    r = verify(appendix_b[i], [other])
    assert not r.ok
    assert [c for c in r.failed() if c.name == 'signature']


@pytest.mark.parametrize('i', range(6))
def test_example_dumps_with_named_fields(appendix_b, i):
    text = dump(appendix_b[i])
    assert 'manifest-sequence-number / 2: %d' % i in text
    assert 'condition-vendor-identifier' in text


def test_published_keys_are_a_pair(spec_key, spec_pub):
    assert keys.anchor(spec_key.public_key()) == keys.anchor(spec_pub)


def test_vendor_id_of_the_examples_is_uuid5_of_arm_com(appendix_b):
    assert ids.vendor_uuid('arm.com').bytes == SPEC_VENDOR
    assert SPEC_VENDOR in appendix_b[1]


def test_namespace_cbor_pen_matches_section_8_4_8_1():
    assert ids.NAMESPACE_CBOR_PEN == uuid.UUID('47fbdabb-f2e4-55f0-bb39-3620c2f6df4e')


def test_our_sig_structure_reproduces_the_examples(appendix_b, spec_pub):
    """Section 8.3 as revised in -38: the detached payload is the SUIT_Digest encoding, no extra wrapper."""
    env = cbor.loads(appendix_b[1]).value
    auth = cbor.loads(env[codes.ENV_AUTH])
    block = cbor.loads(auth[1])
    assert cose.verify1(block, auth[0], [spec_pub]) == 0
    with pytest.raises(cose.CoseError):
        cose.verify1(block, cbor.dumps(auth[0]), [spec_pub])     # one byte-string wrapper too many


def test_example_2_carries_severed_install_and_text(appendix_b, spec_pub):
    r = verify(appendix_b[2], [spec_pub])
    names = {c.name: c.ok for c in r.checks}
    assert names['severed install'] and names['severed text']
