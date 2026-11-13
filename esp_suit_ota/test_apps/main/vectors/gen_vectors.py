#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Generate the compliance vectors of the esp_suit_ota test app.

Each vector is one envelope that follows draft-ietf-suit-manifest-38 except for the one thing its
name says, and the error code the device must return for it. Everything is signed with the key
published in the draft's Appendix B, using deterministic ECDSA, so the output is reproducible:
tools/tests/test_test_app_vectors.py checks that the committed files match a fresh run.

Writes compliance.bin (every envelope and payload, back to back) and compliance_vectors.c (the
index the C tests read). Run it after changing a vector:  python gen_vectors.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'tools'))

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

from esp_suit_tool import codes as C  # noqa: E402
from esp_suit_tool import ids, keys  # noqa: E402
from esp_suit_tool.cbor import Raw, Tag, dumps, head  # noqa: E402
from esp_suit_tool.envelope import Signer, authentication, bstr_digest, digest, envelope  # noqa: E402

SPEC_KEY = keys.load_private(os.path.join(HERE, 'spec_private.pem'))
OTHER_KEY = ec.derive_private_key(0x5EED0F7E57DA7A5EED0F7E57DA7A5EED0F7E57DA7A5EED0F7E57DA7A5EED0F7, ec.SECP256R1())
VENDOR = bytes.fromhex('fa6b4a53d5ad5fdfbe9de663e4d41ffe')     # Appendix B: UUIDv5(DNS, "arm.com")
CLASS = bytes.fromhex('1492af1425695e48bf429b2d51f2ab45')      # Appendix B class
DEVICE = bytes(range(0x10, 0x20))
PEN = 41381
CIDS = [[b'\x00'], [b'\x01'], [b'\x02']]
URL = 'https://test.local/'
MANIFEST_URL = URL + 'manifest.suit'


def _payload(i, n):
    return bytes((i * 31 + k * 7 + (k >> 8)) & 0xff for k in range(n))


P = [_payload(0, 200), _payload(1, 120), _payload(2, 80)]
SMALL = _payload(3, 32)


# ---------- building blocks ----------

def ident(i=0, data=None, **extra):
    """Identity and image parameters of component i."""
    d = P[i] if data is None else data
    p = {C.P_VENDOR: VENDOR, C.P_CLASS: CLASS, C.P_DIGEST: dumps(digest(d)), C.P_SIZE: len(d)}
    p.update(extra)
    return {k: v for k, v in p.items() if v is not None}


CHECKS = [C.COND_VENDOR, C.REP_ALL, C.COND_CLASS, C.REP_ALL]


def shared(n=1, params=None, checks=None):
    seq = []
    for i in range(n):
        if n > 1:
            seq += [C.SET_INDEX, i]
        seq += [C.OVERRIDE, params[i] if params else ident(i)]
    if n > 1:
        seq += [C.SET_INDEX, True]
    return seq + (CHECKS if checks is None else checks)


def fetch(i=0, uri=None):
    return [C.OVERRIDE, {C.P_URI: uri or f'#p{i}'}, C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL]


def install(n=1, uris=None):
    seq = []
    for i in range(n):
        if n > 1:
            seq += [C.SET_INDEX, i]
        seq += fetch(i, uris[i] if uris else None)
    return seq


def validate(n=1):
    return ([C.SET_INDEX, True] if n > 1 else []) + [C.COND_IMAGE_MATCH, C.REP_ALL]


def common(n=1, shared_seq=None, comps=None):
    return dumps({C.COMMON_COMPONENTS: comps if comps is not None else CIDS[:n],
                  C.COMMON_SHARED: dumps(shared(n) if shared_seq is None else shared_seq)})


def manifest(n=1, seq=1, shared_seq=None, install_seq=None, validate_seq=None, comps=None, members=None):
    """The manifest map. members adds or replaces members by key; a value of None removes one."""
    m = {C.MAN_VERSION: 1, C.MAN_SEQUENCE: seq, C.MAN_COMMON: common(n, shared_seq, comps),
         C.MAN_VALIDATE: dumps(validate(n) if validate_seq is None else validate_seq),
         C.MAN_INSTALL: dumps(install(n) if install_seq is None else install_seq)}
    for k, v in (members or {}).items():
        if v is None:
            m.pop(k, None)
        else:
            m[k] = v
    return m


def make(m=None, n=1, integrated=None, severed=None, extra=None, signers=None, man_bytes=None, auth=None,
         tagged=True):
    mb = man_bytes if man_bytes is not None else dumps(manifest(n) if m is None else m)
    a = auth if auth is not None else authentication(mb, signers or [Signer(SPEC_KEY)])
    integ = {f'#p{i}': P[i] for i in range(n)} if integrated is None else integrated
    return envelope(a, mb, severed, integ, extra, tagged)


def raw_map(pairs):
    """A map with entries in exactly the given order, duplicates allowed."""
    return Raw(head(5, len(pairs)) + b''.join(dumps(k) + dumps(v) for k, v in pairs))


def raw_envelope(pairs, tag=True):
    body = raw_map(pairs)
    return (head(6, C.ENVELOPE_TAG) if tag else b'') + bytes(body)


def sign1_block(alg, payload_field=None, tagged=True, tag=C.COSE_SIGN1_TAG, sig=bytes(64)):
    protected = dumps({1: alg})
    arr = [protected, {}, payload_field, sig]
    return dumps(Tag(tag, arr) if tagged else arr)


def with_blocks(mb, blocks):
    d = dumps(bstr_digest(mb))
    return dumps([d] + blocks)


# ---------- the vectors ----------

VECTORS = []          # (name, rule, expect, data, flags)
PAYLOADS = []         # (uri, data) served by the test transport
F_TRANSPORT, F_MANUAL = 1, 2


def vec(name, rule, expect, data, flags=0):
    VECTORS.append((name, rule, expect, bytes(data), flags))


def good():
    # baselines: each exercises REQUIRED functionality and must complete
    vec('good_one_component', '8.4: version, sequence, common, shared, install, validate', 'READY', make())
    vec('good_three_components', '6.5: integer index and True; 8.4.10.1', 'READY', make(manifest(3), n=3))
    vec('good_index_array', '6.5: an array index is REQUIRED with 3 or more components', 'READY',
        make(manifest(3, validate_seq=[C.SET_INDEX, [0, 2], C.COND_IMAGE_MATCH, C.REP_ALL,
                                       C.SET_INDEX, 1, C.COND_IMAGE_MATCH, C.REP_ALL]), n=3))
    vec('good_es256', '8.3 and suit-mti: ES256 (-7)', 'READY', make(signers=[Signer(SPEC_KEY, C.ES256)]))
    vec('good_esp256', '8.3 and suit-mti: ESP256 (-9)', 'READY', make(signers=[Signer(SPEC_KEY, C.ESP256)]))
    vec('good_untrusted_then_trusted_block', '8.3: one block from a trusted key is enough', 'READY',
        make(signers=[Signer(OTHER_KEY), Signer(SPEC_KEY)]))
    vec('good_trusted_then_untrusted_block', '8.3: one block from a trusted key is enough', 'READY',
        make(signers=[Signer(SPEC_KEY), Signer(OTHER_KEY)]))
    ins = dumps(install())
    vec('good_severed_install', '8.6: a severed install sequence, digest in the manifest', 'READY',
        make(manifest(members={C.MAN_INSTALL: bstr_digest(ins)}), severed={C.ENV_INSTALL: ins}))
    text = dumps({'en': {C.TEXT_UPDATE_DESCRIPTION: 'compliance'}})
    vec('good_severed_text', '8.6: a severed text section', 'READY',
        make(manifest(members={C.MAN_TEXT: bstr_digest(text)}), severed={C.ENV_TEXT: text}))
    vec('good_inline_text', '8.4.4: text inside the manifest', 'READY', make(manifest(members={C.MAN_TEXT: text})))
    vec('good_reference_uri', '8.4.3: reference URI is metadata', 'READY', make(manifest(members={C.MAN_REFERENCE_URI: URL + 'r.suit'})))
    vec('good_invoke_sequence_present', '6.1: an updater does not run the invoke procedure', 'READY',
        make(manifest(members={C.MAN_INVOKE: dumps([C.INVOKE, C.REP_ALL])})))
    vec('good_run_sequence', '8.4.10.8: run-sequence', 'READY',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_URI: '#p0'},
                                   C.RUN_SEQUENCE, dumps([C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL])])))
    vec('good_try_each_second_branch', '8.4.10.2: a failed branch moves to the next', 'READY',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_URI: '#p0'},
                                   C.TRY_EACH, [dumps([C.COND_ABORT, C.REP_ALL]),
                                                dumps([C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL])]])))
    vec('good_strict_order', '8.4.8.14: strict-order parameter', 'READY',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_STRICT_ORDER: True}] + install())))
    vec('good_vendor_pen', '8.4.8.3: vendor as cbor-pen #6.112', 'READY',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_VENDOR: ids.pen(PEN)}]))))
    vec('good_device_identifier', '8.4.8.5 and 8.4.9.1: device identifier', 'READY',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_DEVICE: DEVICE}],
                                        CHECKS + [C.COND_DEVICE, C.REP_ALL]))))
    vec('good_envelope_extension', '8.2: $$SUIT_Envelope_Extensions are skipped', 'READY',
        make(extra={99: b'extension'}))
    vec('good_extra_integrated_payload', '7.5: a payload no component asks for is skipped', 'READY',
        make(integrated={'#p0': P[0], '#zz': b'unused'}))
    vec('good_prefetched_full_uri', '7.5: an intermediary encloses a payload under its URI', 'READY',
        make(manifest(install_seq=fetch(0, URL + 'p0.bin')), integrated={URL + 'p0.bin': P[0]}))
    vec('good_transport_fetch', '8.4.10.4: fetch from a URI', 'READY',
        make(manifest(install_seq=fetch(0, URL + 'p0.bin')), integrated={}), F_TRANSPORT)
    vec('good_transport_relative_uri', '8.4.8.10: a relative URI resolves against the manifest', 'READY',
        make(manifest(install_seq=fetch(0, 'p0.bin')), integrated={}), F_TRANSPORT)
    vec('good_transport_three', '8.4.10.4: three fetches in order', 'READY',
        make(manifest(3, install_seq=install(3, [URL + f'p{i}.bin' for i in range(3)])), n=3, integrated={}),
        F_TRANSPORT)
    for i in range(3):
        PAYLOADS.append((URL + f'p{i}.bin', P[i]))


def conformance():
    # section 8.5 tables 9 to 11: each REQUIRED item fails when its check does not hold
    other = bytes(16)
    vec('vendor_mismatch', '8.4.9.1: vendor identifier', 'CONDITION',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_VENDOR: other}]))))
    vec('class_mismatch', '8.4.9.1: class identifier', 'CONDITION',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_CLASS: other}]))))
    p = ident(0)
    del p[C.P_VENDOR]
    vec('vendor_parameter_missing', '8.4.9: a condition without its parameter fails', 'CONDITION',
        make(manifest(shared_seq=shared(1, [p]))))
    p = ident(0)
    del p[C.P_SIZE]
    vec('image_size_missing', '8.4.8.7: image size is REQUIRED', 'SIZE', make(manifest(shared_seq=shared(1, [p]))))
    p = ident(0)
    del p[C.P_DIGEST]
    vec('image_digest_missing', '8.4.8.6: image digest is REQUIRED', 'CONDITION',
        make(manifest(shared_seq=shared(1, [p]))))
    vec('uri_missing', '8.4.10.4: fetch needs a URI', 'CONDITION',
        make(manifest(install_seq=[C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL])))
    vec('image_digest_mismatch', '8.4.9.2: image match', 'DIGEST', make(integrated={'#p0': bytes(len(P[0]))}))
    vec('image_size_mismatch', '8.4.8.7: payload longer than its image size', 'SIZE',
        make(integrated={'#p0': P[0] + b'\x00'}))
    vec('vendor_pen_tag_110', '8.4.8.3: cbor-pen is tag 112, not 110', 'FORMAT',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_VENDOR: Tag(110, ids.pen(PEN).value)}]))))
    vec('vendor_pen_non_minimal_arc', 'RFC 9090: an arc never starts with 0x80', 'FORMAT',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_VENDOR: Tag(C.PEN_TAG, b'\x80' + ids.pen(PEN).value)}]))))
    vec('vendor_pen_mismatch', '8.4.8.3: a PEN this device does not accept', 'CONDITION',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_VENDOR: ids.pen(9)}]))))
    vec('device_mismatch', '8.4.9.1: device identifier', 'CONDITION',
        make(manifest(shared_seq=shared(1, [ident(0) | {C.P_DEVICE: other}], CHECKS + [C.COND_DEVICE, C.REP_ALL]))))
    vec('set_index_out_of_range', '6.5: index beyond the component list', 'UNSUPPORTED',
        make(manifest(3, install_seq=[C.SET_INDEX, 3] + fetch(0)), n=3))
    vec('several_components_without_set_index', '6.3: every sequence MUST begin with set-component-index', 'UNSUPPORTED',
        make(manifest(2, install_seq=fetch(0) + [C.SET_INDEX, 1] + fetch(1)), n=2))
    vec('try_each_exhausted', '8.4.10.2: every branch failed', 'CONDITION',
        make(manifest(install_seq=[C.TRY_EACH, [dumps([C.COND_ABORT, C.REP_ALL]), dumps([C.COND_ABORT, C.REP_ALL])]])))
    vec('condition_abort', '8.4.9.5: abort always fails', 'CONDITION',
        make(manifest(install_seq=[C.COND_ABORT, C.REP_ALL] + install())))
    vec('shared_without_class_check', '6.3: the shared sequence SHOULD check vendor and class', 'UNSUPPORTED',
        make(manifest(shared_seq=shared(1, checks=[C.COND_VENDOR, C.REP_ALL]))))
    vec('fetch_in_shared_sequence', '6.3: the shared sequence holds no directives with effects', 'UNSUPPORTED',
        make(manifest(shared_seq=shared(1) + [C.OVERRIDE, {C.P_URI: '#p0'}, C.FETCH, C.REP_FAIL])))
    # commit reports a failed image match as DIGEST
    vec('soft_failure_then_validate', '8.4.8.15: soft failure ends the sequence, validate then fails', 'DIGEST',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_SOFT_FAILURE: True}, C.COND_ABORT, C.REP_ALL] + install())))


def unsupported():
    # section 6.1 as revised in -38: unsupported functionality is never ignored
    for name, cmd in (('directive_write', C.WRITE), ('directive_copy', C.COPY), ('directive_swap', C.SWAP),
                      ('directive_invoke', C.INVOKE), ('condition_check_content', C.COND_CHECK_CONTENT),
                      ('unknown_command_99', 99), ('custom_command_negative', -300)):
        vec(name, '6.1: an unsupported command MUST abort', 'UNSUPPORTED',
            make(manifest(install_seq=[cmd, C.REP_ALL] + install())))
    vec('unknown_command_with_soft_failure', '6.1 (-38): abort regardless of soft-failure', 'UNSUPPORTED',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_SOFT_FAILURE: True}, 99, C.REP_ALL] + install())))
    vec('unknown_command_in_try_each', '6.1 (-38): abort, do not try the next branch', 'UNSUPPORTED',
        make(manifest(install_seq=[C.OVERRIDE, {C.P_URI: '#p0'},
                                   C.TRY_EACH, [dumps([99, C.REP_ALL]),
                                                dumps([C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL])]])))
    vec('unknown_parameter_99', '6.1: an unsupported parameter', 'UNSUPPORTED',
        make(manifest(install_seq=[C.OVERRIDE, {99: 1}] + install())))
    vec('custom_parameter_negative', '6.1: an unsupported custom parameter', 'UNSUPPORTED',
        make(manifest(install_seq=[C.OVERRIDE, {-300: 1}] + install())))
    vec('unknown_manifest_member', '6.1: an unknown manifest member', 'UNSUPPORTED', make(manifest(members={99: 1})))
    vec('common_dependencies', '6.1: dependencies are not supported', 'UNSUPPORTED',
        make(manifest(members={C.MAN_COMMON: dumps({1: {}, C.COMMON_COMPONENTS: CIDS[:1],
                                                       C.COMMON_SHARED: dumps(shared())})})))
    vec('manifest_without_update_procedure', '6: no install or payload-fetch', 'UNSUPPORTED',
        make(manifest(members={C.MAN_INSTALL: None})))


def setup():
    # section 6.1: processor setup
    vec('manifest_version_2', '6.1: unsupported manifest version', 'UNSUPPORTED', make(manifest(members={C.MAN_VERSION: 2})))
    vec('component_without_target', '6.1: unsupported component identifier', 'MAP',
        make(manifest(comps=[[b'\x09']])))
    vec('duplicate_component', '8.4.5: component identifiers are unique', 'MAP',
        make(manifest(2, comps=[CIDS[0], CIDS[0]]), n=2))
    vec('components_empty', '8.4.5: at least one component', 'FORMAT', make(manifest(comps=[])))


def envelope_rules():
    m = manifest()
    mb = dumps(m)
    good_auth = authentication(mb, [Signer(SPEC_KEY)])
    vec('no_authentication_wrapper', '8.3: the wrapper is REQUIRED', 'AUTH',
        raw_envelope([(C.ENV_MANIFEST, mb), ('#p0', P[0])]))
    vec('manifest_before_authentication', '8.3: the wrapper MUST come before the manifest', 'AUTH',
        raw_envelope([(C.ENV_MANIFEST, mb), (C.ENV_AUTH, good_auth), ('#p0', P[0])]))
    vec('manifest_digest_mismatch', '8.3: digest of the bstr-wrapped manifest', 'AUTH',
        make(auth=authentication(mb, [Signer(SPEC_KEY)], digest_override=[C.SHA256, bytes(32)])))
    vec('manifest_digest_sha512', 'suit-mti: digest algorithm outside the profile', 'UNSUPPORTED',
        make(auth=authentication(mb, [Signer(SPEC_KEY)],
                                 digest_override=[-44, hashlib.sha512(dumps(mb)).digest()])))
    vec('signature_untrusted_key', '8.3: no block from a trusted key', 'AUTH', make(signers=[Signer(OTHER_KEY)]))
    vec('signature_algorithm_eddsa', 'suit-mti: EdDSA is not in this profile', 'UNSUPPORTED',
        make(auth=with_blocks(mb, [sign1_block(-8)])))
    vec('signature_block_untagged', '8.3: COSE_Sign1_Tagged', 'AUTH',
        make(auth=with_blocks(mb, [sign1_block(C.ESP256, tagged=False)])))
    vec('signature_block_mac0', '8.3: a COSE_Mac0 block is not supported', 'UNSUPPORTED',
        make(auth=with_blocks(mb, [sign1_block(C.ESP256, tag=17)])))
    vec('signature_embedded_payload', '8.3 (-38): the payload field is nil', 'AUTH',
        make(auth=with_blocks(mb, [sign1_block(C.ESP256, payload_field=dumps(bstr_digest(mb)))])))
    vec('authentication_without_blocks', '8.3: at least one block', 'AUTH',
        make(auth=dumps([dumps(bstr_digest(mb))])))
    one = bytes(good_auth)
    vec('authentication_claims_more_blocks', 'Appendix A: bstr .cbor SUIT_Authentication, array length', 'AUTH',
        make(auth=bytes([one[0] + 1]) + one[1:]))
    vec('authentication_bytes_after_blocks', 'Appendix A: bstr .cbor holds one item', 'AUTH', make(auth=one + b'\x00'))
    ins = dumps(install())
    vec('severed_install_missing', '8.6: a declared severed member must be present', 'SEVERED',
        make(manifest(members={C.MAN_INSTALL: bstr_digest(ins)})))
    vec('severed_install_wrong_digest', '8.6: severed member digest', 'SEVERED',
        make(manifest(members={C.MAN_INSTALL: [C.SHA256, bytes(32)]}),
             severed={C.ENV_INSTALL: ins}))
    vec('severed_install_not_declared', '8.6: only declared members are severable', 'SEVERED',
        make(severed={C.ENV_INSTALL: ins}))
    text = dumps({'en': {C.TEXT_UPDATE_DESCRIPTION: 'x'}})
    vec('severed_text_wrong_digest', '8.6: severed text digest', 'SEVERED',
        make(manifest(members={C.MAN_TEXT: [C.SHA256, bytes(32)]}), severed={C.ENV_TEXT: text}))
    vec('integrated_key_without_fragment_marker', '7.5: the key is the URI; "p0" is not "#p0"', 'INCOMPLETE',
        make(integrated={'p0': P[0]}))
    vec('integrated_payload_missing', '7.5: the payload the manifest fetches is absent', 'INCOMPLETE',
        make(integrated={}))
    vec('envelope_tag_108', 'Appendix A: the envelope tag is 107', 'FORMAT',
        b'\xd8\x6c' + make()[2:])


def cbor_profile():
    # section 8.1: every map in canonical order; RFC 8949 4.2.1 deterministic encoding
    env = make()
    body = env[2:]
    vec('envelope_indefinite_map', 'RFC 8949 4.2.1: definite lengths only', 'FORMAT',
        b'\xd8\x6b\xbf' + body[1:] + b'\xff')
    vec('envelope_key_not_shortest', 'RFC 8949 4.2.1: shortest integers', 'FORMAT',
        b'\xd8\x6b' + body[:1] + b'\x18\x02' + body[2:])
    m = manifest()
    mb = dumps(m)
    auth = authentication(mb, [Signer(SPEC_KEY)])
    vec('envelope_duplicate_key', 'RFC 8949 5.6: no duplicate keys', 'FORMAT',
        raw_envelope([(C.ENV_AUTH, auth), (C.ENV_MANIFEST, mb), (C.ENV_MANIFEST, mb), ('#p0', P[0])]))
    vec('envelope_bytes_after_end', 'RFC 8949: one data item', 'FORMAT', env + b'\x00')
    vec('envelope_tag_twice', 'Appendix A: one envelope tag', 'FORMAT', b'\xd8\x6b' + env)

    def resigned(name, rule, man_bytes):
        vec(name, rule, 'FORMAT', make(man_bytes=man_bytes))

    resigned('manifest_integer_not_shortest', 'RFC 8949 4.2.1: shortest integers',
             mb.replace(b'\x02\x01', b'\x02\x18\x01', 1))
    resigned('manifest_float', 'RFC 8949 4.2.1 and Appendix A: no floats',
             dumps(m | {C.MAN_SEQUENCE: Raw(bytes.fromhex('fb3ff0000000000000'))}))
    ordered = sorted(m.items(), key=lambda kv: dumps(kv[0]))
    resigned('manifest_keys_out_of_order', '8.1: canonical map order',
             bytes(raw_map([ordered[1], ordered[0]] + ordered[2:])))
    resigned('manifest_duplicate_key', 'RFC 8949 5.6: no duplicate keys',
             bytes(raw_map(ordered[:2] + [ordered[1]] + ordered[2:])))
    resigned('manifest_bstr_trailing_byte', 'Appendix A: bstr .cbor holds one item', mb + b'\x00')
    shared_indef = b'\x9f' + dumps(shared())[1:] + b'\xff'
    resigned('shared_sequence_indefinite', 'RFC 8949 4.2.1: definite lengths only',
             dumps(m | {C.MAN_COMMON: dumps({C.COMMON_COMPONENTS: CIDS[:1], C.COMMON_SHARED: shared_indef})}))
    p = ident(0)
    items = sorted(p.items())
    resigned('parameters_duplicate_key', 'RFC 8949 5.6: no duplicate keys',
             dumps(manifest(shared_seq=[C.OVERRIDE, raw_map(items[:3] + [items[2]] + items[3:])] + CHECKS)))
    resigned('parameters_out_of_order', '8.1: canonical map order',
             dumps(manifest(shared_seq=[C.OVERRIDE, raw_map([items[1], items[0]] + items[2:])] + CHECKS)))


def manual():
    # driven by dedicated C tests, not by the table runner
    small = make(manifest(shared_seq=shared(1, [ident(0, SMALL)])), integrated={'#p0': SMALL})
    vec('robust_small', 'robustness: truncation and single-byte changes', 'READY', small, F_MANUAL)
    # a partition pair: unlike a RAM sink, its installed image can be read back and recognized
    blob = _payload(4, 512)
    pair = [[b'host', b'blob']]
    m = manifest(comps=pair, shared_seq=shared(1, [ident(0, blob)]))
    vec('partition_pair', 'default mapping to a declared pair', 'READY', make(m, integrated={'#p0': blob}), F_MANUAL)
    for seq in (9, 10, 11):
        m = manifest(seq=seq, comps=pair, shared_seq=shared(1, [ident(0, blob)]),
                     install_seq=[C.OVERRIDE, {C.P_URI: '#p0'},
                                  C.TRY_EACH, [dumps([C.COND_IMAGE_MATCH, C.REP_ALL]),
                                               dumps([C.FETCH, C.REP_FAIL, C.COND_IMAGE_MATCH, C.REP_ALL])]])
        vec(f'sequence_{seq}', '6.1: rollback protection', 'READY', make(m, integrated={'#p0': blob}), F_MANUAL)


def generate():
    for part in (good, conformance, unsupported, setup, envelope_rules, cbor_profile, manual):
        part()
    names = [v[0] for v in VECTORS]
    assert len(names) == len(set(names)), 'duplicate vector name'
    blob = b''
    index = []
    for name, rule, expect, data, flags in VECTORS:
        index.append((name, rule, expect, len(blob), len(data), flags))
        blob += data
    pindex = []
    for uri, data in PAYLOADS:
        pindex.append((uri, len(blob), len(data)))
        blob += data
    err = {'READY': 'ESP_SUIT_OTA_READY'}

    def c_str(s):
        return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
    lines = ['/*', ' * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD', ' *',
             ' * SPDX-License-Identifier: Unlicense OR CC0-1.0', ' */',
             '/* Generated by gen_vectors.py from compliance.bin. Do not edit. */',
             '#include "compliance_vectors.h"', '',
             f'const uint8_t suit_cvec_device_id[16] = {{ {", ".join(f"0x{b:02x}" for b in DEVICE)} }};',
             f'const uint32_t suit_cvec_pen = {PEN};', '',
             'const suit_cvec_t suit_cvecs[] = {']
    for name, rule, expect, off, ln, flags in index:
        code = err.get(expect, f'ESP_ERR_SUIT_OTA_{expect}')
        fl = ' | '.join(f for f, b in (('CVEC_TRANSPORT', 1), ('CVEC_MANUAL', 2)) if flags & b) or '0'
        lines.append(f'    {{ {c_str(name)}, {c_str(rule)}, {off}, {ln}, {code}, {fl} }},')
    lines += ['};', 'const size_t suit_cvecs_count = sizeof(suit_cvecs) / sizeof(suit_cvecs[0]);', '',
              'const suit_cpayload_t suit_cpayloads[] = {']
    for uri, off, ln in pindex:
        lines.append(f'    {{ {c_str(uri)}, {off}, {ln} }},')
    lines += ['};', 'const size_t suit_cpayloads_count = sizeof(suit_cpayloads) / sizeof(suit_cpayloads[0]);', '']
    return blob, '\n'.join(lines)


def main():
    blob, source = generate()
    check = '--check' in sys.argv
    out = {'compliance.bin': blob, 'compliance_vectors.c': source.encode()}
    stale = [n for n, data in out.items()
             if not os.path.exists(os.path.join(HERE, n)) or open(os.path.join(HERE, n), 'rb').read() != data]
    if check:
        print('stale: ' + ', '.join(stale) if stale else 'up to date')
        return 1 if stale else 0
    for n, data in out.items():
        open(os.path.join(HERE, n), 'wb').write(data)
    print(f'{len(VECTORS)} vectors, {len(PAYLOADS)} payloads, {len(blob)} bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
