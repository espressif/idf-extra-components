#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Value round-trip check for json_parser: every valid JSONTestSuite document
is parsed by Python, walked through json_parser's public API, and each value
compared. Usage: check_values.py <api_walk-binary> <JSONTestSuite dir>"""
import glob, json, math, os, struct, subprocess, sys

JPX, SUITE = sys.argv[1], sys.argv[2]
INT32 = (-2**31, 2**31 - 1); INT64 = (-2**63, 2**63 - 1)
hx = lambda s: s.encode('utf-8').hex() or "-"

def first_wins(pairs):
    d = {}
    for k, v in pairs:
        d.setdefault(k, v)       # json_parser returns the first match of a duplicated name
    return d

class Walker:
    def __init__(self): self.script, self.expect = [], []   # expect: (label, kind, want)
    def scalar(self, cmd, arg, v, label):
        if isinstance(v, bool):
            self.script.append(f"{cmd['b']} {arg}"); self.expect.append((label, 'b', v))
        elif isinstance(v, int):
            if INT32[0] <= v <= INT32[1]: self.script.append(f"{cmd['i']} {arg}"); self.expect.append((label, 'i', v))
            elif INT64[0] <= v <= INT64[1]: self.script.append(f"{cmd['l']} {arg}"); self.expect.append((label, 'i', v))
            else: self.double(cmd, arg, v, label)
        elif isinstance(v, float):
            self.double(cmd, arg, v, label)
        elif isinstance(v, str):
            self.script.append(f"{cmd['s']} {arg}"); self.script.append(f"{cmd['n']} {arg}")
            if '\x00' in v:   # a C string cannot carry U+0000: the accessors fail
                self.expect += [(label, 'err', None), (label + " strlen", 'err', None)]
            else:
                self.expect += [(label, 's', v), (label + " strlen", 'n', len(v.encode('utf-8')))]
        elif v is None:
            self.script.append(f"{cmd['N']} {arg}"); self.expect.append((label, 'ok', None))
    def double(self, cmd, arg, v, label):
        try: f = float(v)
        except OverflowError: f = math.inf
        self.script.append(f"{cmd['d']} {arg}")
        self.expect.append((label, 'd', f) if math.isfinite(f) else (label, 'err', None))   # outside double: the accessor fails
    def walk(self, v, key=None, idx=None, path="$"):
        OBJ = dict(b='ob', i='oi', l='ol', d='od', s='os', n='sl', N='on'); ARR = dict(b='ab', i='ai', l='aL', d='ad', s='as', n='al', N='an')
        ROOT = dict(b='rb', i='ri', l='rl', d='rd', s='rs', n='rn', N='rN')
        if isinstance(v, dict):
            if key is not None: self.script.append(f"oo {hx(key)}"); self.expect.append((path, 'ok', None))
            elif idx is not None: self.script.append(f"aO {idx}"); self.expect.append((path, 'ok', None))
            for k, x in v.items():
                if '\x00' in k: self.expect.append((f"{path}.{k!r}", 'skip', None)); continue   # NUL in a name: cannot be looked up
                self.walk(x, key=k, path=f"{path}.{k!r}")
            if key is not None: self.script.append("xo"); self.expect.append((path + " leave", 'ok', None))
            elif idx is not None: self.script.append("xO"); self.expect.append((path + " leave", 'ok', None))
        elif isinstance(v, list):
            if key is not None: self.script.append(f"oa {hx(key)}"); self.expect.append((path, 'n', len(v)))
            elif idx is not None: self.script.append(f"aA {idx}"); self.expect.append((path, 'ok', None))
            for i, x in enumerate(v): self.walk(x, idx=i, path=f"{path}[{i}]")
            if key is not None: self.script.append("xa"); self.expect.append((path + " leave", 'ok', None))
            elif idx is not None: self.script.append("xA"); self.expect.append((path + " leave", 'ok', None))
        elif key is not None: self.scalar(OBJ, hx(key), v, path)
        elif idx is not None: self.scalar(ARR, str(idx), v, path)
        else: self.scalar(ROOT, "", v, path)   # lone top-level scalar

def same(kind, want, got):
    if got is None: return False
    if kind == 's': return bytes.fromhex(got[2:]).decode('utf-8', 'replace') == want if got.startswith('s=') else False
    if kind == 'n': return got == f"n={want}"
    if kind == 'i': return got == f"i={want}"
    if kind == 'b': return got == f"b={int(want)}"
    if kind == 'ok': return got == "ok"
    if kind == 'err': return got == "err"
    if kind == 'd': return got.startswith('d=') and struct.pack('>d', float(got[2:])) == struct.pack('>d', want)
    return False

files = sorted(glob.glob(os.path.join(SUITE, "test_parsing", "y_*.json")))
docs, unparsed = [], []
for path in files:
    raw = open(path, 'rb').read()
    try:
        v = json.loads(raw.decode('utf-8'), object_pairs_hook=first_wins)
    except Exception as e:   # Python's decoder is the reference; a document it cannot read is reported, not compared
        unparsed.append(f"{os.path.basename(path)} ({type(e).__name__})"); continue
    w = Walker(); w.walk(v); docs.append((path, w))

script = "".join(f"file {p}\n" + "\n".join(w.script) + ("\n" if w.script else "") + "end\n" for p, w in docs)
run = subprocess.run([JPX], input=script.encode(), capture_output=True)
if run.returncode != 0:   # a sanitizer report, for instance, arrives here after otherwise complete output
    sys.stderr.write(run.stderr.decode('utf-8', 'replace')); print(f"api_walk exited with {run.returncode}"); sys.exit(1)
out = run.stdout.decode('utf-8', 'replace').split("\n")

pos, total_docs, ok_docs, fails, skipped = 0, 0, 0, [], 0
for path, w in docs:
    name = os.path.basename(path); total_docs += 1
    opened = out[pos]; pos += 1
    results = []
    for (label, kind, want) in w.expect:
        if kind == 'skip': skipped += 1; continue
        results.append((label, kind, want, out[pos])); pos += 1
    assert out[pos] == "--", (name, out[pos]); pos += 1
    if opened != "ok":
        fails.append((name, "json_parse_start", 'ok', 'ok', opened)); continue
    bad = [(l, k, wt, g) for (l, k, wt, g) in results if not same(k, wt, g)]
    if bad: fails.append((name,) + bad[0])   # (name, label, kind, want, got)
    else: ok_docs += 1

print(f"{ok_docs} / {total_docs} valid documents read back correctly through the public API  ({skipped} NUL-containing names not looked up)")
for u in unparsed: print(f"  not compared, reference parser failed: {u}")
for name, label, kind, want, got in fails[:40]:
    print(f"  FAIL {name:<52} {label} want={want!r} got={got!r}")
if len(fails) > 40: print(f"  ... {len(fails) - 40} more")
sys.exit(1 if fails or unparsed else 0)
