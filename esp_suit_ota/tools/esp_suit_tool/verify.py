# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check an envelope the way the device will, before it is published."""
from __future__ import annotations

import hashlib
from dataclasses import dataclass, field

from . import codes, cose
from .cbor import DecodeError, Tag, dumps, is_deterministic, loads


@dataclass
class Check:
    name: str
    ok: bool | None                     # None: not checked
    detail: str = ''


@dataclass
class Report:
    checks: list = field(default_factory=list)
    components: list = field(default_factory=list)   # {'id', 'uri', 'size', 'digest'} per component
    sequence_number: int | None = None

    @property
    def ok(self):
        return all(c.ok is not False for c in self.checks) and any(c.ok for c in self.checks)

    def add(self, name, ok, detail=''):
        self.checks.append(Check(name, ok, detail))
        return ok

    def failed(self):
        return [c for c in self.checks if c.ok is False]

    def __str__(self):
        mark = {True: 'ok  ', False: 'FAIL', None: 'skip'}
        return '\n'.join(f'{mark[c.ok]}  {c.name}' + (f': {c.detail}' if c.detail else '') for c in self.checks)


def _params(comps, sequences):
    """Parameters per component after running the given command sequences in order."""
    params = [dict() for _ in comps]

    def run(seq_bytes, index):
        seq = loads(seq_bytes)
        for i in range(0, len(seq) - 1, 2):
            cmd, arg = seq[i], seq[i + 1]
            if cmd == codes.SET_INDEX:
                index = list(range(len(comps))) if arg is True else (arg if isinstance(arg, list) else [arg])
            elif cmd == codes.OVERRIDE:
                for j in index:
                    if 0 <= j < len(params):
                        params[j].update(arg)
            elif cmd == codes.TRY_EACH:
                for branch in arg:
                    if branch is not None:
                        run(branch, index)
            elif cmd == codes.RUN_SEQUENCE:
                run(arg, index)
        return index

    for seq in sequences:
        run(seq, [0])
    return params


def _sequence_encoding(name, data, bad):
    """Check a command sequence and every bstr .cbor inside it: try-each, run-sequence, image digests."""
    if not is_deterministic(data):
        bad.append(name)
        return
    seq = loads(data)
    if not isinstance(seq, list):
        bad.append(name)
        return
    for i in range(0, len(seq) - 1, 2):
        cmd, arg = seq[i], seq[i + 1]
        if cmd == codes.TRY_EACH and isinstance(arg, list):
            for n, b in enumerate(arg):
                if isinstance(b, bytes):
                    _sequence_encoding(f'{name} try-each branch {n}', b, bad)
        elif cmd == codes.RUN_SEQUENCE and isinstance(arg, bytes):
            _sequence_encoding(f'{name} run-sequence', arg, bad)
        elif cmd == codes.OVERRIDE and isinstance(arg, dict) and isinstance(arg.get(codes.P_DIGEST), bytes):
            if not is_deterministic(arg[codes.P_DIGEST]):
                bad.append(f'{name} image-digest')


def verify(env_bytes, public_keys=(), payloads=None, vendor=None, class_id=None):
    """Report on env_bytes. payloads maps a URI to the bytes the device will fetch from it.

    Never raises on a malformed envelope: whatever cannot be decoded is a failed check.
    """
    r = Report()
    try:
        _verify(r, env_bytes, public_keys, payloads, vendor, class_id)
    except (DecodeError, ValueError, TypeError, KeyError, IndexError, AttributeError) as e:
        r.add('structure', False, f'malformed: {e}')
    return r


def _verify(r, env_bytes, public_keys, payloads, vendor, class_id):
    top = loads(env_bytes)
    tagged = isinstance(top, Tag) and top.tag == codes.ENVELOPE_TAG
    r.add('envelope tag 107', tagged, '' if tagged else 'a SUIT envelope is #6.107(map)')
    env = top.value if isinstance(top, Tag) else top
    if not isinstance(env, dict):
        r.add('envelope is a map', False)
        return
    if not r.add('deterministic CBOR', is_deterministic(env_bytes),
                 'RFC 8949 4.2.1: shortest heads, definite lengths, sorted keys, no duplicates'):
        return
    missing = [k for k in (codes.ENV_AUTH, codes.ENV_MANIFEST) if k not in env]
    if not r.add('authentication wrapper and manifest present', not missing,
                 f'missing {missing}' if missing else ''):
        return

    # authentication
    auth = loads(env[codes.ENV_AUTH])
    if not (isinstance(auth, list) and len(auth) >= 2 and all(isinstance(b, bytes) for b in auth)):
        r.add('authentication wrapper', False, 'needs a digest and at least one COSE_Sign1')
        return
    d = loads(auth[0])
    want = hashlib.sha256(dumps(env[codes.ENV_MANIFEST])).digest()
    r.add('manifest digest', isinstance(d, list) and d == [codes.SHA256, want],
          'SHA-256 over the bstr-wrapped manifest')
    if public_keys:
        good = []
        for n, b in enumerate(auth[1:]):
            try:
                k = cose.verify1(loads(b), auth[0], public_keys)
                good.append(f'block {n} with key {k}')
            except (cose.CoseError, DecodeError, ValueError) as e:
                r.add(f'signature block {n}', None, str(e))
        r.add('signature', bool(good), ', '.join(good) if good else 'no block verifies with a given key')
    else:
        r.add('signature', None, 'no key given')

    # manifest
    manifest = loads(env[codes.ENV_MANIFEST])
    nested = [('manifest', env[codes.ENV_MANIFEST]), ('authentication wrapper', env[codes.ENV_AUTH]),
              ('digest', auth[0])] + [(f'signature block {n}', b) for n, b in enumerate(auth[1:])]
    nested += [('common', manifest.get(codes.MAN_COMMON))]
    nested += [(codes.MAN_NAMES[k], manifest[k]) for k in (codes.MAN_VALIDATE, codes.MAN_LOAD, codes.MAN_INVOKE,
                                                         codes.MAN_PAYLOAD_FETCH, codes.MAN_INSTALL, codes.MAN_TEXT)
               if isinstance(manifest.get(k), bytes)]
    bad = [n for n, v in nested if v is not None and not is_deterministic(v)]
    if 'common' not in bad and isinstance(manifest.get(codes.MAN_COMMON), bytes):
        shared = loads(manifest[codes.MAN_COMMON]).get(codes.COMMON_SHARED)
        if isinstance(shared, bytes):
            _sequence_encoding('shared-sequence', shared, bad)
    for k in (codes.MAN_VALIDATE, codes.MAN_LOAD, codes.MAN_INVOKE, codes.MAN_PAYLOAD_FETCH, codes.MAN_INSTALL):
        seq_bytes = manifest.get(k) if isinstance(manifest.get(k), bytes) else (
            env.get(k) if isinstance(manifest.get(k), list) and isinstance(env.get(k), bytes) else None)
        if seq_bytes is not None and codes.MAN_NAMES[k] not in bad:
            _sequence_encoding(codes.MAN_NAMES[k], seq_bytes, bad)
    r.add('deterministic CBOR inside the members', not bad, ', '.join(bad))
    r.add('manifest version 1', manifest.get(codes.MAN_VERSION) == 1)
    seq = manifest.get(codes.MAN_SEQUENCE)
    r.sequence_number = seq if isinstance(seq, int) else None
    r.add('sequence number', isinstance(seq, int) and seq >= 0, str(seq))

    # severed members: declared by digest in the manifest, carried in the envelope
    for k in codes.SEVERABLE:
        in_man, in_env = manifest.get(k), env.get(k)
        name = codes.ENV_NAMES[k]
        if isinstance(in_man, list):
            if in_env is None:
                r.add(f'severed {name}', False, 'the manifest holds its digest but the envelope does not carry it')
            else:
                r.add(f'severed {name}', isinstance(in_env, bytes) and
                      in_man == [codes.SHA256, hashlib.sha256(dumps(in_env)).digest()] and is_deterministic(in_env),
                      'digest of the bstr-wrapped section')
        elif in_env is not None:
            r.add(f'severed {name}', False, 'the envelope carries it but the manifest did not sever it')
    extra = [k for k in env if isinstance(k, int) and k not in (codes.ENV_AUTH, codes.ENV_MANIFEST) + codes.SEVERABLE]
    if extra:
        r.add('envelope extensions', None, f'unknown members {extra} will be skipped by the device')

    # components: identity and payloads
    install = manifest.get(codes.MAN_INSTALL)
    install_bytes = install if isinstance(install, bytes) else env.get(codes.ENV_INSTALL)
    try:
        common = loads(manifest[codes.MAN_COMMON])
        comps = common.get(codes.COMMON_COMPONENTS, [])
        params = _params(comps, [common[codes.COMMON_SHARED]] + ([install_bytes] if install_bytes else []))
    except (DecodeError, ValueError, TypeError, KeyError, IndexError) as e:
        r.add('command sequences', False, f'cannot follow them: {e}')
        return
    for cid, p in zip(comps, params):
        dg = loads(p[codes.P_DIGEST]) if isinstance(p.get(codes.P_DIGEST), bytes) else None
        r.components.append({'id': [x.decode(errors='replace') for x in cid], 'uri': p.get(codes.P_URI),
                             'size': p.get(codes.P_SIZE), 'digest': dg[1].hex() if dg else None})
    if vendor is not None:
        r.add('vendor identifier', all(p.get(codes.P_VENDOR) == vendor for p in params))
    if class_id is not None:
        r.add('class identifier', all(p.get(codes.P_CLASS) == class_id for p in params))

    def check_payload(label, data, comp):
        ok = comp['size'] == len(data) and comp['digest'] == hashlib.sha256(data).hexdigest()
        r.add(label, ok, f'{len(data)} bytes' if ok else
              f'{len(data)} bytes sha256 {hashlib.sha256(data).hexdigest()[:16]}..., '
              f'manifest says {comp["size"]} bytes {str(comp["digest"])[:16]}...')

    by_uri = {c['uri']: c for c in r.components if c['uri'] is not None}
    for key, data in env.items():
        if isinstance(key, str):
            comp = by_uri.get(key)
            if comp is None:
                r.add(f'integrated "{key}"', False, 'no component has this URI, the device would ignore it')
            else:
                check_payload(f'integrated "{key}"', data, comp)
    for uri in by_uri:
        if uri.startswith('#') and uri not in env:
            r.add(f'integrated "{uri}"', False, 'a fragment URI names a payload inside the envelope, and it is absent')
    for uri, data in (payloads or {}).items():
        comp = by_uri.get(uri)
        if comp is None:
            r.add(f'payload "{uri}"', False, 'no component has this URI')
        else:
            check_payload(f'payload "{uri}"', data, comp)
