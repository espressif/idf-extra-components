# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""An envelope in CBOR diagnostic notation, with every code point named."""
from . import codes
from .cbor import Tag, loads


def _hx(b, limit=48):
    s = b.hex()
    return "h'" + (s if len(s) <= limit else s[:limit] + '...') + "'"


def _value(v):
    if isinstance(v, bytes):
        return _hx(v)
    if isinstance(v, Tag):
        return f'{v.tag}({_value(v.value)})'
    if isinstance(v, str):
        return f'"{v}"'
    if v is True or v is False:
        return str(v).lower()
    if isinstance(v, list):
        return '[' + ', '.join(_value(x) for x in v) + ']'
    return repr(v)


def _digest(d):
    return f'[ {d[0]} / {codes.ALG_NAMES.get(d[0], "?")} /, {_hx(d[1], 64)} ]'


def _params(m, ind):
    out = []
    for k, v in m.items():
        name = codes.PARAM_NAMES.get(k, f'custom({k})')
        if k == codes.P_DIGEST and isinstance(v, bytes):
            out.append(f'{ind}/ {name} / {k}: << {_digest(loads(v))} >>,')
        else:
            out.append(f'{ind}/ {name} / {k}: {_value(v)},')
    return '\n'.join(out)


def sequence(data, ind):
    seq = loads(data)
    lines = []
    for i in range(0, len(seq) - 1, 2):
        cmd, arg = seq[i], seq[i + 1]
        name = codes.CMD_NAMES.get(cmd, f'custom({cmd})')
        if cmd == codes.OVERRIDE and isinstance(arg, dict):
            lines.append(f'{ind}/ {name} / {cmd}, {{\n{_params(arg, ind + "    ")}\n{ind}}},')
        elif cmd == codes.TRY_EACH and isinstance(arg, list):
            lines.append(f'{ind}/ {name} / {cmd}, [')
            for b in arg:
                lines.append(f'{ind}    nil,' if b is None else f'{ind}    << [\n{sequence(b, ind + "        ")}\n{ind}    ] >>,')
            lines.append(f'{ind}],')
        elif cmd == codes.RUN_SEQUENCE and isinstance(arg, bytes):
            lines.append(f'{ind}/ {name} / {cmd}, << [\n{sequence(arg, ind + "    ")}\n{ind}] >>,')
        else:
            lines.append(f'{ind}/ {name} / {cmd}, {_value(arg)},')
    return '\n'.join(lines)


def dump(env_bytes):
    top = loads(env_bytes)
    env = top.value if isinstance(top, Tag) else top
    out = [f'{top.tag if isinstance(top, Tag) else ""}({{  / {len(env_bytes)} bytes /']
    auth = loads(env[codes.ENV_AUTH])
    out.append('  / authentication-wrapper / 2: << [')
    out.append(f'    / digest / << {_digest(loads(auth[0]))} >>,')
    for b in auth[1:]:
        s1 = loads(b)
        prot = loads(s1.value[0]) if s1.value[0] else {}
        alg = prot.get(1)
        out.append(f'    / COSE_Sign1 / << {s1.tag}([ << {{ / alg / 1: {alg} / {codes.ALG_NAMES.get(alg, "?")} / }} >>, '
                   f'{_value(s1.value[1])}, nil, {_hx(s1.value[3], 32)} ]) >>,')
    out.append('  ] >>,')
    man = loads(env[codes.ENV_MANIFEST])
    out.append('  / manifest / 3: << {')
    for k, v in man.items():
        name = codes.MAN_NAMES.get(k, str(k))
        if k == codes.MAN_COMMON:
            common = loads(v)
            out.append('    / common / 3: << {')
            for ck, cv in common.items():
                if ck == codes.COMMON_SHARED:
                    out.append(f'      / shared-sequence / 4: << [\n{sequence(cv, "        ")}\n      ] >>,')
                elif ck == codes.COMMON_COMPONENTS:
                    out.append(f'      / components / 2: {_value(cv)},')
                else:
                    out.append(f'      / {codes.COMMON_NAMES.get(ck, ck)} / {ck}: {_value(cv)},')
            out.append('    } >>,')
        elif isinstance(v, list) and k in codes.SEVERABLE:
            out.append(f'    / {name} / {k}: {_digest(v)}  / severed /,')
        elif isinstance(v, bytes) and k in (7, 8, 9, 16, 20):
            out.append(f'    / {name} / {k}: << [\n{sequence(v, "      ")}\n    ] >>,')
        elif k == codes.MAN_TEXT:
            out.append(f'    / text / 23: << {loads(v)!r} >>,')
        else:
            out.append(f'    / {name} / {k}: {_value(v)},')
    out.append('  } >>,')
    for k, v in env.items():
        if k in (codes.ENV_AUTH, codes.ENV_MANIFEST):
            continue
        if k == codes.ENV_TEXT:
            out.append(f'  / text / 23: << {loads(v)!r} >>,')
        elif isinstance(k, int) and k in codes.SEVERABLE:
            out.append(f'  / {codes.ENV_NAMES[k]} / {k}: << [\n{sequence(v, "    ")}\n  ] >>,')
        elif isinstance(k, str):
            out.append(f'  / integrated payload / "{k}": {_hx(v, 24)} / {len(v)} bytes /,')
        else:
            out.append(f'  / extension / {k}: {_value(v)},')
    out.append('})')
    return '\n'.join(out)
