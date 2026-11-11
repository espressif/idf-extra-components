# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""RFC 8949 section 4.2.1 core deterministic encoding, the profile SUIT requires."""
from collections.abc import Mapping

import cbor2
import pytest

from esp_suit_tool import cbor


@pytest.mark.parametrize('value,encoded', [
    (0, '00'), (23, '17'), (24, '1818'), (255, '18ff'), (256, '190100'), (65535, '19ffff'),
    (65536, '1a00010000'), (2**32 - 1, '1affffffff'), (2**32, '1b0000000100000000'),
    (-1, '20'), (-24, '37'), (-25, '3818'), (-16, '2f'),
])
def test_integers_take_the_shortest_head(value, encoded):
    assert cbor.dumps(value).hex() == encoded


def test_strings_bool_null():
    assert cbor.dumps(b'') == b'\x40'
    assert cbor.dumps(b'\x01' * 24)[:2] == b'\x58\x18'
    assert cbor.dumps('a') == b'\x61a'
    assert cbor.dumps([True, False, None]) == b'\x83\xf5\xf4\xf6'


def test_map_keys_sort_bytewise_not_length_first():
    # RFC 8949: 1000 (19 03e8) sorts before "a" (61 61). RFC 7049 length-first puts "a" first.
    assert cbor.dumps({'a': 2, 1000: 1}).hex() == 'a21903e801616102'
    assert cbor2.dumps({'a': 2, 1000: 1}, canonical=True).hex() == 'a26161021903e801'
    # every unsigned key sorts before every negative key
    assert cbor.dumps({-1: 0, 24: 0}).hex() == 'a2181800' + '2000'


def test_duplicate_key_is_refused():
    class Pairs(Mapping):
        def __init__(self, items):
            self._items = items

        def __getitem__(self, k):
            return dict(self._items)[k]

        def __iter__(self):
            return iter(k for k, _ in self._items)

        def __len__(self):
            return len(self._items)

        def items(self):
            return list(self._items)

    with pytest.raises(ValueError):
        cbor.dumps(Pairs([(1, 'x'), (1, 'y')]))


def test_float_is_not_in_the_profile():
    with pytest.raises(TypeError):
        cbor.dumps(1.5)


def test_raw_is_copied_verbatim():
    assert cbor.dumps([cbor.Raw(b'\x18\x01')]) == b'\x81\x18\x01'


def test_tag():
    assert cbor.dumps(cbor.Tag(107, {})) == b'\xd8\x6b\xa0'


@pytest.mark.parametrize('hexdata,why', [
    ('bf0102ff', 'indefinite-length map'),
    ('1801', 'integer not in its shortest form'),
    ('5801aa', 'length not in its shortest form'),
    ('fb3ff8000000000000', 'a float'),
    ('a26161021903e801', 'keys in length-first order'),
    ('a201000101', 'a duplicate key'),
    ('0000', 'bytes after the item'),
])
def test_is_deterministic_rejects(hexdata, why):
    assert not cbor.is_deterministic(bytes.fromhex(hexdata)), why


def test_is_deterministic_accepts_its_own_output():
    obj = {1: [b'\x00', 'x', -7], 'k': cbor.Tag(112, b'\x82\x01'), 23: {True: None}}
    assert cbor.is_deterministic(cbor.dumps(obj))


def test_loads_gives_plain_types_inside_tags():
    v = cbor.loads(cbor.dumps(cbor.Tag(18, [b'', {1: [2]}, None, b''])))
    assert isinstance(v.value, list) and isinstance(v.value[1], dict) and isinstance(v.value[1][1], list)
