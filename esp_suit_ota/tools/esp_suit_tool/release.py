# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Build a signed envelope from a release description.

release.yml (paths are relative to the YAML file):

  sequence_number: 42                  # or auto-utc: seconds since 1970
  vendor: espressif.com                # UUIDv5(DNS, domain); or vendor_pen: 41381 to send cbor-pen
  class: my-product                    # UUIDv5(vendor UUID, class), section 8.4.8.2
  reference_uri: https://ota.example.com/rel.suit      # optional
  text: { en: { update-description: "1.2.3: fixes" } } # optional, severed unless inline_text
  components:
    - { id: [esp32c5, app], file: build/app.bin, uri: app.bin, skip_if_installed: true }
    - { id: [esp32c5, cfg], file: cfg.bin,       uri: "#cfg" }   # "#name": carried in the envelope
  signing:
    keys: [keys/manifest.pem]          # one COSE_Sign1 per key; the device needs one it trusts
    alg: ESP256                        # or ES256
    kid: mfg01                         # optional
"""
from __future__ import annotations

import hashlib
import os
import re
import time
from dataclasses import dataclass, field

from . import codes, ids, keys
from .cbor import dumps
from .envelope import Signer, authentication, bstr_digest, digest, entry_offsets, envelope

MAX_COMPONENTS = 16


class ReleaseError(Exception):
    pass


@dataclass
class Component:
    id: list                            # list of bytes
    data: bytes
    uri: str
    skip_if_installed: bool = False
    fetch_args: bytes | None = None
    enclose: bool = False               # carry the payload in the envelope under its URI


@dataclass
class Release:
    sequence_number: int
    vendor: object                      # 16 UUID bytes or a cbor-pen tag
    class_id: bytes
    components: list
    reference_uri: str | None = None
    text: dict | None = None            # {language: {text key: value}}
    signers: list = field(default_factory=list)


@dataclass
class Built:
    envelope: bytes
    manifest: bytes
    info: dict


def read_floor(build_dir):
    """CONFIG_ESP_SUIT_OTA_INITIAL_SEQ from a build directory, so a release never goes below it."""
    if not build_dir:
        return 0
    for cfg in (os.path.join(build_dir, 'sdkconfig'), os.path.join(os.path.dirname(build_dir.rstrip('/')), 'sdkconfig')):
        if os.path.exists(cfg):
            m = re.search(r'^CONFIG_ESP_SUIT_OTA_INITIAL_SEQ=(\d+)', open(cfg).read(), re.M)
            return int(m.group(1)) if m else 0
    return 0


def _part(p):
    return p.encode() if isinstance(p, str) else bytes(p)


def from_spec(spec, base_dir='.', key_paths=None, alg=None, min_seq=0):
    """A Release from a parsed release.yml. Relative paths resolve against base_dir."""
    def path(p):
        return p if os.path.isabs(p) else os.path.join(base_dir, p)

    for k in ('class', 'components'):
        if k not in spec:
            raise ReleaseError(f'release description has no "{k}"')
    if not spec.get('vendor') and not spec.get('vendor_pen'):
        raise ReleaseError('release description needs "vendor" (a domain) or "vendor_pen" (an enterprise number)')
    comps = spec['components'] or []
    if not comps:
        raise ReleaseError('no components')
    if len(comps) > MAX_COMPONENTS:
        raise ReleaseError(f'more than {MAX_COMPONENTS} components')
    seq = spec.get('sequence_number', 'auto-utc')
    seq = int(time.time()) if seq == 'auto-utc' else int(seq)
    if seq < min_seq:
        raise ReleaseError(f'sequence number {seq} is below the floor {min_seq}')

    if spec.get('vendor_pen'):
        vendor = ids.pen(int(spec['vendor_pen']))
        klass = ids.pen_class_uuid(int(spec['vendor_pen']), spec['class'])
    else:
        vendor = ids.vendor_uuid(spec['vendor']).bytes
        klass = ids.class_uuid(ids.vendor_uuid(spec['vendor']), spec['class'])
    components = []
    for c in comps:
        uri = c['uri']
        components.append(Component(
            id=[_part(p) for p in c['id']],
            data=open(path(c['file']), 'rb').read(),
            uri=uri,
            skip_if_installed=bool(c.get('skip_if_installed', False)),
            fetch_args=_part(c['fetch_args']) if 'fetch_args' in c else None,
            enclose=uri.startswith('#') or bool(c.get('prefetch', False))))
    seen = set()
    for c in components:
        if tuple(c.id) in seen:
            raise ReleaseError(f'component {c.id} is listed twice')
        seen.add(tuple(c.id))

    signing = spec.get('signing', {}) or {}
    paths = key_paths or signing.get('keys') or ([signing['key']] if signing.get('key') else [])
    if not paths:
        raise ReleaseError('no signing key: set signing.keys in the release or pass --key')
    alg_name = alg or signing.get('alg', 'ESP256')
    if alg_name not in codes.SIGNATURE_ALGS:
        raise ReleaseError(f'signing.alg must be one of {", ".join(codes.SIGNATURE_ALGS)}')
    kid = signing.get('kid')
    signers = [Signer(keys.load_private(path(p) if key_paths is None else p), codes.SIGNATURE_ALGS[alg_name],
                      kid.encode() if isinstance(kid, str) else kid) for p in paths]

    text = None
    if spec.get('text'):
        text = {lang: {codes.TEXT_KEYS.get(k, k): v for k, v in fields.items()} for lang, fields in spec['text'].items()}

    return Release(sequence_number=seq,
                   vendor=vendor,
                   class_id=klass.bytes, components=components, reference_uri=spec.get('reference_uri'),
                   text=text, signers=signers)


def load(yaml_path, key_paths=None, alg=None, min_seq=0):
    import yaml
    spec = yaml.safe_load(open(yaml_path))
    if not isinstance(spec, dict):
        raise ReleaseError(f'{yaml_path}: not a release description')
    return from_spec(spec, os.path.dirname(os.path.abspath(yaml_path)), key_paths, alg, min_seq)


def sequences(r):
    """The shared, install and validate command sequences for a release.

    Draft section 6.3: with more than one component, every sequence begins with set-component-index.
    The shared sequence sets each component's identity and image parameters, then checks vendor and
    class for all of them. Install fetches each one, or skips it when the image already matches.
    """
    multi = len(r.components) > 1
    shared = []
    for i, c in enumerate(r.components):
        if multi:
            shared += [codes.SET_INDEX, i]
        shared += [codes.OVERRIDE, {codes.P_VENDOR: r.vendor, codes.P_CLASS: r.class_id,
                                    codes.P_DIGEST: dumps(digest(c.data)), codes.P_SIZE: len(c.data)}]
    if multi:
        shared += [codes.SET_INDEX, True]
    shared += [codes.COND_VENDOR, codes.REP_ALL, codes.COND_CLASS, codes.REP_ALL]

    install = []
    for i, c in enumerate(r.components):
        if multi:
            install += [codes.SET_INDEX, i]
        params = {codes.P_URI: c.uri}
        if c.fetch_args is not None:
            params[codes.P_FETCH_ARGS] = c.fetch_args
        install += [codes.OVERRIDE, params]
        fetch = [codes.FETCH, codes.REP_FAIL, codes.COND_IMAGE_MATCH, codes.REP_ALL]
        if c.skip_if_installed:
            install += [codes.TRY_EACH, [dumps([codes.COND_IMAGE_MATCH, codes.REP_ALL]), dumps(fetch)]]
        else:
            install += fetch

    validate = ([codes.SET_INDEX, True] if multi else []) + [codes.COND_IMAGE_MATCH, codes.REP_ALL]
    return shared, install, validate


def build(r, sever_install=False, inline_text=False):
    shared, install, validate = sequences(r)
    common = dumps({codes.COMMON_COMPONENTS: [c.id for c in r.components], codes.COMMON_SHARED: dumps(shared)})
    manifest = {codes.MAN_VERSION: 1, codes.MAN_SEQUENCE: r.sequence_number, codes.MAN_COMMON: common,
                codes.MAN_VALIDATE: dumps(validate)}
    if r.reference_uri:
        manifest[codes.MAN_REFERENCE_URI] = r.reference_uri
    severed = {}
    install_bytes = dumps(install)
    if sever_install:
        manifest[codes.MAN_INSTALL] = bstr_digest(install_bytes)
        severed[codes.ENV_INSTALL] = install_bytes
    else:
        manifest[codes.MAN_INSTALL] = install_bytes
    if r.text:
        text_bytes = dumps(r.text)
        if inline_text:
            manifest[codes.MAN_TEXT] = text_bytes
        else:
            manifest[codes.MAN_TEXT] = bstr_digest(text_bytes)
            severed[codes.ENV_TEXT] = text_bytes
    manifest_bytes = dumps(manifest)
    integrated = {c.uri: c.data for c in r.components if c.enclose}
    env = envelope(authentication(manifest_bytes, r.signers), manifest_bytes, severed, integrated)
    vendor = r.vendor.hex() if isinstance(r.vendor, bytes) else f'pen:{r.vendor.value.hex()}'
    info = {
        'sequence_number': r.sequence_number,
        'vendor_id': vendor,
        'class_id': r.class_id.hex(),
        'envelope_size': len(env),
        'entries': entry_offsets(env),
        'components': [{'id': [p.decode(errors='replace') for p in c.id], 'uri': c.uri, 'size': len(c.data),
                        'sha256': hashlib.sha256(c.data).hexdigest(), 'enclosed': c.enclose}
                       for c in r.components],
    }
    return Built(env, manifest_bytes, info)
