#!/usr/bin/env python3
"""Assert that the ESP-IDF default port stays strippable from the link.

esp_schedule reaches the outside world through the function-pointer tables in
include/esp_schedule.h. The ESP-IDF implementations live in port/esp/, and
src/esp_schedule_default.c - which holds esp_schedule_init() and nothing else -
is their only referrer.

That is the whole link-strip guarantee. An application calling
esp_schedule_init_with_config() and never esp_schedule_init() leaves
esp_schedule_default.o unreferenced, so the linker never pulls it out of
libesp_schedule.a, and the default port stays out of the image along with the
FreeRTOS-timer, nvs_flash, esp_sntp and esp_log code it would have dragged in.
It works at archive-member granularity, so it does not depend on --gc-sections.

It is also fragile in a way no compiler warns about, and it has two halves:

  1. No second referrer *inside the component*. Naming an esp_schedule_esp_*_ops
     table from any other file here makes the tables reachable from an object
     every caller already needs. Application code is free to name them - that is
     what esp_schedule_esp_port.h is for, and it only links what it names. The
     guarantee is that the component never forces the choice.

  2. No second reason to link esp_schedule_default.o. The guarantee rests on
     esp_schedule_init() being the *only* external symbol in that object. Adding
     any other non-static function or variable to it - a version getter, a
     helper, a counter - gives every caller of that symbol a reason to pull the
     member in, and the default port comes with it.

Half 1 is checked across the component; half 2 is checked in the one file.

Checked at source level rather than with nm on a built ELF because demonstrating
the strip needs an application that installs a non-ESP port: the component's own
test_app calls esp_schedule_init(), so the default port is legitimately present
in its image and nm would have nothing to assert.

Known limits, so the next reader does not over-trust this:
  - Half 2 relies on the repo's astyle formatting (definitions start at column
    zero, OTBS braces). It is a lint, not a C parser. A definition written in an
    unusual layout can slip past it.
  - Neither half sees through macros. A reference assembled by token pasting is
    not detected.
  - Comments and string literals are stripped before matching, so discussing a
    table name in prose is fine and quoting one is not a reference.

Usage:
    check_port_isolation.py [--root <component-dir>]
"""

import argparse
import re
import sys
from pathlib import Path

# Referencing any of these forces the corresponding port/esp object into the link.
PORT_SYMBOL_RE = re.compile(r'\besp_schedule_esp_(?:\w+_ops|log)\b')

# The declaration site, and the one definition site permitted to name them.
ALLOWED = {
    Path('src/esp_schedule_default.c'),
    Path('include/esp_schedule_esp_port.h'),
}

# port/esp/*.c each define their own table; that is the definition, not a
# cross-reference, and those files are already inside the strippable set.
ALLOWED_DIRS = {Path('port/esp')}

SEARCH_DIRS = ('src', 'include', 'port')

# The file whose external surface must stay at exactly one symbol, and that symbol.
SOLE_REFERRER = Path('src/esp_schedule_default.c')
SOLE_EXPORT = 'esp_schedule_init'

# A definition at file scope: no leading whitespace, and not a continuation.
# Deliberately conservative - see "Known limits" above.
EXTERNAL_DEF_RE = re.compile(
    r'''^(?!\s)                     # column zero
        (?!static\b|extern\b|typedef\b)
        (?P<decl>[A-Za-z_][^;{()]*?)   # return type / type and qualifiers
        \b(?P<name>[A-Za-z_]\w*)\s*
        (?:\((?P<params>[^;{]*)\)\s*\{ # function definition (body follows)
          |=                           # or an initialised object definition
        )''',
    re.VERBOSE | re.MULTILINE)


def strip_comments_and_strings(text):
    """Blank out comments and string/char literals, preserving line structure.

    Replacing rather than deleting keeps every line number intact, so a reported
    line matches the file. Necessary because a naive "does the line start with a
    comment marker" filter is wrong in both directions: it skips `*p = &table;`
    as if it were a comment continuation, and it fails to skip a single-line
    `/* table */`.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        two = text[i:i + 2]
        if two == '/*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(c if c == '\n' else ' ' for c in text[i:j]))
            i = j
        elif two == '//':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif ch in '"\'':
            j = i + 1
            while j < n and text[j] != ch:
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(''.join(c if c == '\n' else ' ' for c in text[i:j]))
            i = j
        else:
            out.append(ch)
            i += 1
    return ''.join(out)


def check_no_second_referrer(root):
    """Half 1: only the allowed files may name an esp_schedule_esp_*_ops table."""
    offenders, checked = [], 0
    for directory in SEARCH_DIRS:
        for path in sorted((root / directory).rglob('*')):
            if path.suffix not in ('.c', '.h') or not path.is_file():
                continue
            rel = path.relative_to(root)
            checked += 1
            if rel in ALLOWED or rel.parent in ALLOWED_DIRS:
                continue
            code = strip_comments_and_strings(path.read_text())
            for lineno, line in enumerate(code.splitlines(), 1):
                match = PORT_SYMBOL_RE.search(line)
                if match:
                    offenders.append((rel, lineno, match.group(0), line.strip()))
    return offenders, checked


def check_sole_export(root):
    """Half 2: esp_schedule_default.c must define nothing but esp_schedule_init."""
    path = root / SOLE_REFERRER
    if not path.is_file():
        return [(SOLE_REFERRER, 0, '<missing>', 'file not found')]
    code = strip_comments_and_strings(path.read_text())
    extras = []
    for match in EXTERNAL_DEF_RE.finditer(code):
        name = match.group('name')
        if name == SOLE_EXPORT:
            continue
        lineno = code.count('\n', 0, match.start()) + 1
        extras.append((SOLE_REFERRER, lineno, name, match.group(0).strip()))
    return extras


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=Path(__file__).parent, type=Path,
                    help="component directory (default: this script's directory)")
    args = ap.parse_args()

    offenders, checked = check_no_second_referrer(args.root)
    extras = check_sole_export(args.root)

    if not offenders and not extras:
        print(f'PASS: only {SOLE_REFERRER} and port/esp/ name the default port tables,')
        print(f'      and {SOLE_REFERRER} exports nothing but {SOLE_EXPORT}()')
        print(f'      checked {checked} sources')
        return 0

    print('FAIL: the default ESP-IDF port is no longer strippable from the link')

    if offenders:
        print('\n  Referenced outside its single referrer:')
        for rel, lineno, symbol, line in offenders:
            print(f'    {rel}:{lineno}: {symbol}')
            print(f'      {line}')
        print('\n    Any such reference links the default port - and FreeRTOS')
        print('    timers, nvs_flash, esp_sntp and esp_log - into every build,')
        print('    including applications that only call')
        print('    esp_schedule_init_with_config(). Keep the reference in')
        print(f'    {SOLE_REFERRER}, or add the new file to ALLOWED here if it')
        print('    is genuinely part of the default port.')

    if extras:
        print(f'\n  Extra external symbols in {SOLE_REFERRER}:')
        for rel, lineno, name, line in extras:
            print(f'    {rel}:{lineno}: {name}')
            print(f'      {line}')
        print(f'\n    {SOLE_REFERRER} must export only {SOLE_EXPORT}(). Any other')
        print('    external symbol gives its callers a reason to pull this')
        print('    object out of the archive, which links the default port even')
        print('    into applications that never call esp_schedule_init(). Put')
        print('    the new symbol in another file, or make it static.')

    return 1


if __name__ == '__main__':
    sys.exit(main())
