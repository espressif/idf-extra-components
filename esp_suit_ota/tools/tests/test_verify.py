# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""verify() catches each way an envelope can be wrong, and names the check that failed."""
import cbor2
import pytest

from esp_suit_tool import cbor, codes, ids, release, verify
from esp_suit_tool.envelope import Signer, authentication, envelope


@pytest.fixture
def built(workdir):
    return release.build(release.load(str(workdir / 'release.yml')))


def _env(b):
    return cbor.loads(b).value


def _failed(r):
    return {c.name for c in r.failed()}


def _resign(spec_key, man_bytes, env, over=None):
    """Rebuild an envelope from parts, signing whatever manifest bytes are given."""
    rest = {k: v for k, v in env.items() if k not in (codes.ENV_AUTH, codes.ENV_MANIFEST)}
    rest.update(over or {})
    return envelope(authentication(man_bytes, [Signer(spec_key)]), man_bytes, extra=rest)


def test_good(built, spec_pub):
    assert verify(built.envelope, [spec_pub]).ok


def test_flipped_signature_byte(built, spec_pub):
    env = _env(built.envelope)
    auth = cbor.loads(env[codes.ENV_AUTH])
    block = cbor.loads(auth[1])
    block.value[3] = bytes([block.value[3][0] ^ 1]) + block.value[3][1:]
    env[codes.ENV_AUTH] = cbor.dumps([auth[0], cbor.dumps(block)])
    assert 'signature' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_manifest_changed_after_signing(built, spec_pub):
    env = _env(built.envelope)
    man = cbor.loads(env[codes.ENV_MANIFEST])
    man[codes.MAN_SEQUENCE] = 8
    env[codes.ENV_MANIFEST] = cbor.dumps(man)
    assert 'manifest digest' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_severed_section_missing(built, spec_pub):
    env = _env(built.envelope)
    del env[codes.ENV_TEXT]
    assert 'severed text' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_severed_section_changed(built, spec_pub):
    env = _env(built.envelope)
    env[codes.ENV_TEXT] = cbor.dumps({'en': {2: 'other text'}})
    assert 'severed text' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_severed_section_not_declared(built, spec_key, spec_pub):
    env = _env(built.envelope)
    out = _resign(spec_key, env[codes.ENV_MANIFEST], env, {codes.ENV_INSTALL: cbor.dumps([])})
    assert 'severed install' in _failed(verify(out, [spec_pub]))


def test_integrated_payload_changed(built, spec_pub):
    env = _env(built.envelope)
    env['#cfg'] = b'X' + env['#cfg'][1:]
    assert 'integrated "#cfg"' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_integrated_payload_nobody_asked_for(built, spec_pub):
    env = _env(built.envelope)
    env['#nobody'] = b'1234'
    assert 'integrated "#nobody"' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_fragment_without_marker_is_another_uri(built, spec_pub):
    """Section 7.5: the key is the URI. "cfg" does not name the "#cfg" component."""
    env = _env(built.envelope)
    env['cfg'] = env.pop('#cfg')
    assert 'integrated "cfg"' in _failed(verify(cbor.dumps(cbor.Tag(107, env)), [spec_pub]))


def test_untagged_envelope(built, spec_pub):
    env = _env(built.envelope)
    assert 'envelope tag 107' in _failed(verify(cbor.dumps(env), [spec_pub]))


def test_length_first_key_order_is_not_deterministic(built, spec_pub):
    env = _env(built.envelope)
    env[1000] = b''                       # bytewise 1000 (19 03e8) sorts before "a" (61 61),
    env['a'] = b''                        # length-first puts the shorter "a" first
    data = cbor2.dumps(cbor2.CBORTag(107, env), canonical=True)
    assert 'deterministic CBOR' in _failed(verify(data, [spec_pub]))


def test_non_shortest_head_inside_the_manifest(built, spec_key, spec_pub):
    env = _env(built.envelope)
    man = cbor.loads(env[codes.ENV_MANIFEST])
    raw = cbor.dumps(man)
    raw = raw.replace(b'\x02\x07', b'\x02\x18\x07', 1)    # sequence number 7 as 18 07
    assert 'deterministic CBOR inside the members' in _failed(verify(_resign(spec_key, raw, env), [spec_pub]))


def test_missing_manifest(built):
    env = _env(built.envelope)
    del env[codes.ENV_MANIFEST]
    assert 'authentication wrapper and manifest present' in _failed(verify(cbor.dumps(cbor.Tag(107, env))))


def test_payload_files(built, workdir, spec_pub):
    app = (workdir / 'app.bin').read_bytes()
    assert verify(built.envelope, [spec_pub], payloads={'app.bin': app}).ok
    assert 'payload "app.bin"' in _failed(verify(built.envelope, [spec_pub], payloads={'app.bin': app[:-1]}))
    assert 'payload "x.bin"' in _failed(verify(built.envelope, [spec_pub], payloads={'x.bin': app}))


def test_identity(built, spec_pub):
    v = ids.vendor_uuid('espressif.com')
    assert verify(built.envelope, [spec_pub], vendor=v.bytes, class_id=ids.class_uuid(v, 'tool-test').bytes).ok
    r = verify(built.envelope, [spec_pub], vendor=v.bytes, class_id=ids.class_uuid(v, 'other').bytes)
    assert _failed(r) == {'class identifier'}


def test_a_report_with_nothing_checked_is_not_ok():
    assert not verify(b'\xff').ok


def test_components_listed(built):
    r = verify(built.envelope)
    assert [c['uri'] for c in r.components] == ['app.bin', '#cfg']
    assert r.components[1]['size'] == 106
