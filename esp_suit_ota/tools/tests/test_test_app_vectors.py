# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""The test app's compliance vectors: up to date, and judged the same way by the tool and the device."""
import importlib.util
import os

import pytest

from esp_suit_tool import verify

GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'test_apps', 'main', 'vectors',
                   'gen_vectors.py')


@pytest.fixture(scope='module')
def gen():
    spec = importlib.util.spec_from_file_location('gen_vectors', GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.generate()
    return mod


def test_committed_vectors_match_the_generator():
    spec = importlib.util.spec_from_file_location('gen_vectors_check', GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    blob, source = mod.generate()
    here = os.path.dirname(GEN)
    assert open(os.path.join(here, 'compliance.bin'), 'rb').read() == blob, 'run gen_vectors.py'
    assert open(os.path.join(here, 'compliance_vectors.c')).read() == source, 'run gen_vectors.py'


def test_every_vector_the_device_accepts_passes_the_tool(gen, spec_pub):
    served = dict(gen.PAYLOADS)
    served.update({u.replace(gen.URL, ''): d for u, d in gen.PAYLOADS})         # relative URIs
    for name, _rule, expect, data, flags in gen.VECTORS:
        # the tool, unlike the device, flags a payload no component asks for
        if expect != 'READY' or name == 'good_extra_integrated_payload':
            continue
        payloads = None
        if flags & gen.F_TRANSPORT:
            payloads = {c['uri']: served[c['uri']] for c in verify(data).components}
        r = verify(data, [spec_pub], payloads=payloads)
        assert r.ok, f'{name}\n{r}'


@pytest.mark.parametrize('prefix', ['envelope_indefinite', 'envelope_key_not_shortest', 'envelope_duplicate',
                                    'envelope_bytes_after', 'manifest_integer', 'manifest_float',
                                    'manifest_keys_out_of_order', 'manifest_duplicate', 'manifest_bstr_trailing',
                                    'shared_sequence_indefinite', 'parameters_duplicate', 'parameters_out_of_order'])
def test_every_encoding_rule_vector_fails_the_tool(gen, spec_pub, prefix):
    hits = [(n, d) for n, _r, _e, d, _f in gen.VECTORS if n.startswith(prefix)]
    assert hits
    for name, data in hits:
        assert not verify(data, [spec_pub]).ok, name


def test_authentication_vectors_fail_the_tool(gen, spec_pub):
    for name, _r, _e, data, _f in gen.VECTORS:
        if name.startswith(('signature_', 'authentication_', 'manifest_digest', 'no_authentication',
                            'severed_', 'integrated_payload_missing', 'integrated_key_without')):
            assert not verify(data, [spec_pub]).ok, name
