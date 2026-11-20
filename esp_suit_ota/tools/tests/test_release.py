# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Envelopes built from a release description follow the draft's structure rules."""
import hashlib

import pytest

from esp_suit_tool import cbor, codes, ids, keys, release, verify
from esp_suit_tool.envelope import entry_offsets


def _manifest(env_bytes):
    env = cbor.loads(env_bytes).value
    return env, cbor.loads(env[codes.ENV_MANIFEST])


def _sequences(env, man):
    common = cbor.loads(man[codes.MAN_COMMON])
    install = man[codes.MAN_INSTALL] if isinstance(man[codes.MAN_INSTALL], bytes) else env[codes.ENV_INSTALL]
    return {'shared': cbor.loads(common[codes.COMMON_SHARED]), 'install': cbor.loads(install),
            'validate': cbor.loads(man[codes.MAN_VALIDATE])}


def test_release_verifies(workdir, spec_pub):
    built = release.build(release.load(str(workdir / 'release.yml')))
    r = verify(built.envelope, [spec_pub])
    assert r.ok, str(r)
    assert r.sequence_number == 7


def test_manifest_structure(workdir):
    env, man = _manifest(release.build(release.load(str(workdir / 'release.yml'))).envelope)
    assert man[codes.MAN_VERSION] == 1 and man[codes.MAN_SEQUENCE] == 7
    common = cbor.loads(man[codes.MAN_COMMON])
    assert common[codes.COMMON_COMPONENTS] == [[b'esp32c5', b'app'], [b'esp32c5', b'cfg']]
    assert set(env) == {codes.ENV_AUTH, codes.ENV_MANIFEST, codes.ENV_TEXT, '#cfg'}


def test_every_sequence_begins_with_set_component_index(workdir):
    """Section 6.3: with more than one component, every command sequence begins with it."""
    env, man = _manifest(release.build(release.load(str(workdir / 'release.yml'))).envelope)
    for name, seq in _sequences(env, man).items():
        assert seq[0] == codes.SET_INDEX, name


def test_shared_sequence_checks_vendor_and_class(workdir):
    """Section 6.3: the shared sequence carries the vendor and class conditions."""
    env, man = _manifest(release.build(release.load(str(workdir / 'release.yml'))).envelope)
    shared = _sequences(env, man)['shared']
    assert codes.COND_VENDOR in shared[0::2] and codes.COND_CLASS in shared[0::2]


def test_single_component_needs_no_index(workdir):
    y = workdir / 'one.yml'
    y.write_text('sequence_number: 1\nvendor: espressif.com\nclass: c\n'
                 'components: [ { id: [x], file: cfg.bin, uri: "#cfg" } ]\nsigning: { keys: [key.pem] }\n')
    env, man = _manifest(release.build(release.load(str(y))).envelope)
    assert _sequences(env, man)['shared'][0] == codes.OVERRIDE


def test_skip_if_installed_is_a_try_each(workdir):
    env, man = _manifest(release.build(release.load(str(workdir / 'release.yml'))).envelope)
    install = _sequences(env, man)['install']
    i = install.index(codes.TRY_EACH)
    branches = install[i + 1]
    assert cbor.loads(branches[0]) == [codes.COND_IMAGE_MATCH, codes.REP_ALL]
    assert cbor.loads(branches[1])[0] == codes.FETCH


def test_integrated_payload_is_carried_under_its_uri(workdir):
    env, _ = _manifest(release.build(release.load(str(workdir / 'release.yml'))).envelope)
    assert env['#cfg'] == (workdir / 'cfg.bin').read_bytes()


def test_text_is_severed_unless_inline(workdir, spec_pub):
    r = release.load(str(workdir / 'release.yml'))
    env, man = _manifest(release.build(r).envelope)
    assert isinstance(man[codes.MAN_TEXT], list) and codes.ENV_TEXT in env
    env, man = _manifest(release.build(r, inline_text=True).envelope)
    assert isinstance(man[codes.MAN_TEXT], bytes) and codes.ENV_TEXT not in env
    assert cbor.loads(man[codes.MAN_TEXT]) == {'en': {codes.TEXT_UPDATE_DESCRIPTION: 'tool test'}}


def test_sever_install(workdir, spec_pub):
    built = release.build(release.load(str(workdir / 'release.yml')), sever_install=True)
    env, man = _manifest(built.envelope)
    assert man[codes.MAN_INSTALL] == [codes.SHA256, hashlib.sha256(cbor.dumps(env[codes.ENV_INSTALL])).digest()]
    assert verify(built.envelope, [spec_pub]).ok


def test_algorithms(workdir, spec_pub):
    for name, alg in codes.SIGNATURE_ALGS.items():
        built = release.build(release.load(str(workdir / 'release.yml'), alg=name))
        auth = cbor.loads(cbor.loads(built.envelope).value[codes.ENV_AUTH])
        assert cbor.loads(cbor.loads(auth[1]).value[0]) == {1: alg}
        assert verify(built.envelope, [spec_pub]).ok


def test_default_algorithm_is_esp256(workdir):
    built = release.build(release.load(str(workdir / 'release.yml')))
    auth = cbor.loads(cbor.loads(built.envelope).value[codes.ENV_AUTH])
    assert cbor.loads(cbor.loads(auth[1]).value[0]) == {1: codes.ESP256}


def test_two_signers_each_verify(workdir, spec_pub):
    other = keys.generate()
    keys.write_private(other, str(workdir / 'other.pem'))
    built = release.build(release.load(str(workdir / 'release.yml'),
                                       key_paths=[str(workdir / 'key.pem'), str(workdir / 'other.pem')]))
    auth = cbor.loads(cbor.loads(built.envelope).value[codes.ENV_AUTH])
    assert len(auth) == 3
    assert verify(built.envelope, [spec_pub]).ok
    assert verify(built.envelope, [other.public_key()]).ok


def test_builds_are_reproducible(workdir):
    a = release.build(release.load(str(workdir / 'release.yml'))).envelope
    b = release.build(release.load(str(workdir / 'release.yml'))).envelope
    assert a == b


def test_vendor_pen_uses_tag_112_and_the_pen_class_rule(workdir, spec_pub):
    y = workdir / 'pen.yml'
    y.write_text('sequence_number: 1\nvendor_pen: 41381\nclass: c\n'
                 'components: [ { id: [x], file: cfg.bin, uri: "#cfg" } ]\nsigning: { keys: [key.pem] }\n')
    env, man = _manifest(release.build(release.load(str(y))).envelope)
    params = cbor.loads(cbor.loads(man[codes.MAN_COMMON])[codes.COMMON_SHARED])[1]
    assert params[codes.P_VENDOR] == cbor.Tag(codes.PEN_TAG, bytes([0x82, 0xc3, 0x25]))
    assert params[codes.P_CLASS] == ids.pen_class_uuid(41381, 'c').bytes


def test_entry_offsets_point_at_each_member(workdir):
    built = release.build(release.load(str(workdir / 'release.yml')))
    env = cbor.loads(built.envelope).value
    for key, e in entry_offsets(built.envelope).items():
        k = int(key) if key.lstrip('-').isdigit() else key
        assert built.envelope[e['content_offset']:e['content_offset'] + e['content_size']] == env[k]
        assert built.envelope[e['offset']:e['offset'] + e['size']] == cbor.dumps(env[k])


@pytest.mark.parametrize('yaml,message', [
    ('vendor: v\nclass: c\ncomponents: []\nsigning: { keys: [key.pem] }\n', 'no components'),
    ('class: c\ncomponents: [ { id: [x], file: cfg.bin, uri: a } ]\nsigning: { keys: [key.pem] }\n', 'vendor'),
    ('vendor: v\nclass: c\ncomponents: [ { id: [x], file: cfg.bin, uri: a } ]\n', 'no signing key'),
    ('vendor: v\nclass: c\ncomponents: [ { id: [x], file: cfg.bin, uri: a }, { id: [x], file: cfg.bin, uri: b } ]\n'
     'signing: { keys: [key.pem] }\n', 'listed twice'),
    ('vendor: v\nclass: c\ncomponents: [ { id: [x], file: cfg.bin, uri: a } ]\n'
     'signing: { keys: [key.pem], alg: EdDSA }\n', 'signing.alg'),
    ('sequence_number: 3\nvendor: v\nclass: c\ncomponents: [ { id: [x], file: cfg.bin, uri: a } ]\n'
     'signing: { keys: [key.pem] }\n', 'below the floor'),
])
def test_bad_release_is_refused(workdir, yaml, message):
    y = workdir / 'bad.yml'
    y.write_text(yaml)
    with pytest.raises(release.ReleaseError, match=message):
        release.load(str(y), min_seq=5)


def test_rsa_key_is_refused(workdir):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    k = rsa.generate_private_key(65537, 2048)
    (workdir / 'rsa.pem').write_bytes(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                                      serialization.NoEncryption()))
    with pytest.raises(keys.KeyFileError, match='P-256'):
        release.load(str(workdir / 'release.yml'), key_paths=[str(workdir / 'rsa.pem')])


def test_floor_from_sdkconfig(tmp_path):
    (tmp_path / 'sdkconfig').write_text('CONFIG_FOO=y\nCONFIG_ESP_SUIT_OTA_INITIAL_SEQ=1700000000\n')
    assert release.read_floor(str(tmp_path)) == 1700000000
    assert release.read_floor(str(tmp_path / 'build')) == 1700000000   # the project next to its build dir
    assert release.read_floor(str(tmp_path / 'a' / 'b')) == 0
