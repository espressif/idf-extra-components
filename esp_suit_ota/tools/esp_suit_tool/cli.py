# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""esp-suit: make the keys, identifiers and signed SUIT envelopes the esp_suit_ota component consumes.

  esp-suit keygen  manifest.pem --anchor anchor_p256.bin   new signing key and the device's trust anchor
  esp-suit anchor  manifest.pem -o anchor_p256.bin         trust anchor of an existing key
  esp-suit ids     --vendor espressif.com --class my-board  vendor and class UUIDs as a C header
  esp-suit create  release.yml                             signed envelope from a release description
  esp-suit verify  release.suit --key anchor_p256.bin      check it the way the device will
  esp-suit inspect release.suit                            print it with every field named
"""
import argparse
import json
import os
import sys

from . import codes, cose, ids, keys, release
from .dump import dump
from .verify import verify


def _fail(msg):
    print(f'esp-suit: {msg}', file=sys.stderr)
    return 2


def cmd_keygen(a):
    if os.path.exists(a.key) and not a.force:
        return _fail(f'{a.key} exists, pass --force to replace it')
    k = keys.generate()
    keys.write_private(k, a.key)
    print(f'wrote {a.key} (P-256 private key, keep it secret)')
    if a.anchor:
        open(a.anchor, 'wb').write(keys.anchor(k.public_key()))
        print(f'wrote {a.anchor} (65-byte trust anchor for the device)')
    return 0


def cmd_anchor(a):
    point = keys.anchor(keys.load_public(a.key))
    if a.output:
        open(a.output, 'wb').write(point)
        print(f'wrote {a.output} ({len(point)} bytes)')
    if a.c_array:
        print(keys.c_array(a.c_array, point), end='')
    if not a.output and not a.c_array:
        print(point.hex())
    return 0


def cmd_ids(a):
    if (a.vendor is None) == (a.pen is None):
        return _fail('give exactly one of --vendor and --pen')
    if a.pen is not None:
        c = ids.pen_class_uuid(a.pen, a.klass)
        header = ids.c_header_pen(a.pen, c, a.klass)
        summary = f'vendor PEN {a.pen}, class {c}'
    else:
        v = ids.vendor_uuid(a.vendor)
        c = ids.class_uuid(v, a.klass)
        header = ids.c_header(v, c, a.vendor, a.klass)
        summary = f'vendor {v}, class {c}'
    if a.output:
        open(a.output, 'w').write(header)
        print(f'wrote {a.output}: {summary}')
    else:
        print(header, end='')
    return 0


def cmd_create(a):
    floor = max(a.min_seq, release.read_floor(a.build_dir))
    r = release.load(a.release, key_paths=a.key, alg=a.alg, min_seq=floor)
    built = release.build(r, sever_install=a.sever_install, inline_text=a.inline_text)
    out = a.output or os.path.splitext(a.release)[0] + '.suit'
    open(out, 'wb').write(built.envelope)
    report = verify(built.envelope, [s.key.public_key() for s in r.signers])
    if not report.ok:
        return _fail('the envelope just built does not verify:\n' + str(report))
    info = a.info or os.path.splitext(out)[0] + '.json'
    json.dump(built.info, open(info, 'w'), indent=2)
    print(f'wrote {out} ({len(built.envelope)} bytes, sequence {r.sequence_number}, '
          f'{len(r.components)} components) and {info}')
    return 0


def cmd_verify(a):
    pubs = [keys.load_public(p) for p in a.key or []]
    payloads = {}
    for item in a.payload or []:
        if '=' not in item:
            return _fail(f'--payload takes URI=FILE, not {item}')
        uri, path = item.split('=', 1)
        payloads[uri] = open(path, 'rb').read()
    vendor = ids.vendor_uuid(a.vendor).bytes if a.vendor else None
    klass = ids.class_uuid(ids.vendor_uuid(a.vendor), a.klass).bytes if a.vendor and a.klass else None
    report = verify(open(a.envelope, 'rb').read(), pubs, payloads, vendor, klass)
    print(report)
    if report.sequence_number is not None:
        print(f'sequence {report.sequence_number}, {len(report.components)} components')
    if report.ok and not pubs:
        print('note: no --key given, so the signature was not checked')
    return 0 if report.ok else 1


def cmd_inspect(a):
    data = open(a.envelope, 'rb').read()
    if a.json:
        r = verify(data)
        print(json.dumps({'size': len(data), 'sequence_number': r.sequence_number, 'components': r.components},
                         indent=2))
    else:
        print(dump(data))
    return 0


def parser():
    ap = argparse.ArgumentParser(prog='esp-suit', description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True, metavar='COMMAND')

    p = sub.add_parser('keygen', help='create a P-256 signing key')
    p.add_argument('key', help='private key PEM to write')
    p.add_argument('--anchor', metavar='FILE', help='also write the 65-byte trust anchor')
    p.add_argument('--force', action='store_true', help='replace an existing key file')
    p.set_defaults(func=cmd_keygen)

    p = sub.add_parser('anchor', help='trust anchor of a key, for the device')
    p.add_argument('key', help='PEM private or public key')
    p.add_argument('-o', '--output', metavar='FILE', help='write the 65 bytes to FILE')
    p.add_argument('--c-array', metavar='NAME', help='print a C initializer named NAME')
    p.set_defaults(func=cmd_anchor)

    p = sub.add_parser('ids', help='vendor and class UUIDs as a C header')
    p.add_argument('--vendor', help='vendor domain, for example espressif.com')
    p.add_argument('--pen', type=int, help='vendor private enterprise number, instead of --vendor')
    p.add_argument('--class', dest='klass', required=True, help='class name, for example my-board-rev2')
    p.add_argument('-o', '--output', metavar='FILE', help='write the header to FILE')
    p.set_defaults(func=cmd_ids)

    p = sub.add_parser('create', help='signed envelope from a release YAML')
    p.add_argument('release', help='release description (YAML)')
    p.add_argument('-o', '--output', metavar='FILE', help='envelope path (default: release name + .suit)')
    p.add_argument('--info', metavar='FILE', help='JSON summary path (default: envelope name + .json)')
    p.add_argument('--key', action='append', metavar='PEM', help='signing key, repeatable; replaces signing.keys')
    p.add_argument('--alg', choices=sorted(codes.SIGNATURE_ALGS), help='signature algorithm (default ESP256)')
    p.add_argument('--sever-install', action='store_true', help='carry the install sequence outside the manifest')
    p.add_argument('--inline-text', action='store_true', help='keep the text inside the manifest')
    p.add_argument('--min-seq', type=int, default=0, help='refuse a sequence number below this')
    p.add_argument('--build-dir', help='also refuse one below CONFIG_ESP_SUIT_OTA_INITIAL_SEQ of this build')
    p.set_defaults(func=cmd_create)

    p = sub.add_parser('verify', help='check an envelope the way the device will')
    p.add_argument('envelope')
    p.add_argument('--key', action='append', metavar='FILE', help='trusted key: PEM or 65-byte anchor, repeatable')
    p.add_argument('--payload', action='append', metavar='URI=FILE', help='check a payload the device will fetch')
    p.add_argument('--vendor', help='expected vendor domain')
    p.add_argument('--class', dest='klass', help='expected class name (needs --vendor)')
    p.set_defaults(func=cmd_verify)

    p = sub.add_parser('inspect', help='print an envelope with every field named')
    p.add_argument('envelope')
    p.add_argument('--json', action='store_true', help='print the components as JSON instead')
    p.set_defaults(func=cmd_inspect)
    return ap


def main(argv=None):
    a = parser().parse_args(argv)
    try:
        return a.func(a)
    except (release.ReleaseError, keys.KeyFileError, OSError) as e:
        return _fail(str(e))


if __name__ == '__main__':
    sys.exit(main())
