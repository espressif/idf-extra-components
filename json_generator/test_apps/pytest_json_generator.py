# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import json
import math

import pytest
from pytest_embedded_idf.utils import idf_parametrize

# One entry per document printed by print_corpus() in json_generator_test.c,
# in the same order. 'ERR' means the generator must refuse to produce it.
EXPECTED = [
    {'k': 'he said "hi"'},
    {'path': 'C:\\temp'},
    {'k': 'line1\nline2'},
    {'k': 'a\x01b'},
    {'ev"il': 'x'},
    {'k': 'caf\u00e9 \u6e2c'},
    'ERR',  # invalid UTF-8 must not be emitted
    {'nan': None, 'inf': None, 'big': 1e30, 'tenth': 0.1, 'huge': 1e300, 'tiny': 5e-324},
    {'k': 'part1 " part2\u00e9'},
]


def _reject_constants(name: str) -> None:
    # json.loads() accepts NaN/Infinity by default; RFC 8259 does not
    raise ValueError(f'non-standard constant {name} in output')


@pytest.mark.generic
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_json_generator(dut) -> None:
    got = []
    while True:
        match = dut.expect(r'@@(JSON|ERR|CORPUS_END)(?::([^\r\n]*))?\r?\n', timeout=30)
        kind = match.group(1).decode()
        if kind == 'CORPUS_END':
            break
        got.append((kind, match.group(2).decode('utf-8') if match.group(2) else ''))

    assert len(got) == len(EXPECTED), f'corpus size mismatch: {got}'
    for (kind, text), want in zip(got, EXPECTED):
        if want == 'ERR':
            assert kind == 'ERR', f'expected an error, got {text!r}'
            continue
        assert kind == 'JSON', f'expected a document, got error {text}'
        parsed = json.loads(text, parse_constant=_reject_constants)
        for key, value in want.items():
            if isinstance(value, float):
                # float32 values are printed with %.5f; compare within that precision
                assert math.isclose(parsed[key], value, rel_tol=1e-5), f'{key}: {parsed[key]} != {value}'
            else:
                assert parsed[key] == value, f'{key}: {parsed[key]!r} != {value!r}'
        assert parsed.keys() == want.keys()

    dut.run_all_single_board_cases()
