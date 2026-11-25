# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""A release description is JSON, and the tool reads the whole of JSON."""
import json

import pytest

from conftest import run_cli
from esp_suit_tool import ids, keys, release

SPEC = {
    'sequence_number': 7,
    'vendor': 'example.com',
    'class': 'my-board-rev2',
    'components': [{'id': ['esp32c5', 'app'], 'file': 'fw.bin', 'uri': '#fw'}],
    'signing': {'keys': ['sign.pem']},
}


@pytest.fixture
def jsondir(tmp_path):
    keys.write_private(keys.generate(), str(tmp_path / 'sign.pem'))
    (tmp_path / 'fw.bin').write_bytes(bytes(range(256)))
    return tmp_path


def build(path):
    return release.build(release.load(str(path)))


# --- what JSON allows -------------------------------------------------------

def test_indented_with_tabs(jsondir):
    (jsondir / 'r.json').write_text(json.dumps(SPEC, indent='\t'))
    assert build(jsondir / 'r.json').info['sequence_number'] == 7


def test_written_on_one_line(jsondir):
    (jsondir / 'r.json').write_text(json.dumps(SPEC, separators=(',', ':')))
    assert build(jsondir / 'r.json').info['sequence_number'] == 7


def test_layout_does_not_change_the_envelope(jsondir):
    (jsondir / 'a.json').write_text(json.dumps(SPEC, indent=4))
    (jsondir / 'b.json').write_text(json.dumps(SPEC, separators=(',', ':')))
    assert build(jsondir / 'a.json').envelope == build(jsondir / 'b.json').envelope


def test_non_ascii_text_in_any_escaping(jsondir):
    """\\uXXXX escapes and raw UTF-8 are the same string, so they give the same envelope."""
    spec = dict(SPEC, text={'de': {'update-description': 'Grüße'}})
    (jsondir / 'escaped.json').write_text(json.dumps(spec, ensure_ascii=True))
    (jsondir / 'raw.json').write_text(json.dumps(spec, ensure_ascii=False), encoding='utf-8')
    assert build(jsondir / 'escaped.json').envelope == build(jsondir / 'raw.json').envelope


def test_a_class_name_that_looks_like_a_word(jsondir):
    """A YAML 1.1 parser read no as False and hashed an empty string. JSON cannot."""
    for word in ('no', 'yes', 'on', 'off', 'null', 'true'):
        (jsondir / 'r.json').write_text(json.dumps(dict(SPEC, **{'class': word})))
        want = ids.class_uuid(ids.vendor_uuid('example.com'), word).bytes.hex()
        assert build(jsondir / 'r.json').info['class_id'] == want


def test_a_number_as_a_class_is_refused(jsondir):
    (jsondir / 'r.json').write_text(json.dumps(dict(SPEC, **{'class': 1.20})))
    with pytest.raises(release.ReleaseError, match='must be text'):
        release.load(str(jsondir / 'r.json'))


# --- what JSON refuses ------------------------------------------------------

def test_a_syntax_error_names_json(jsondir):
    (jsondir / 'broken.json').write_text('{ "sequence_number": 7, }')
    with pytest.raises(release.ReleaseError, match='not valid JSON'):
        release.load(str(jsondir / 'broken.json'))


def test_a_comment_is_a_syntax_error(jsondir):
    """JSON has no comments. The error says so rather than dropping the line."""
    (jsondir / 'c.json').write_text('{\n  // the app\n  "class": "c"\n}')
    with pytest.raises(release.ReleaseError, match='not valid JSON'):
        release.load(str(jsondir / 'c.json'))


def test_a_top_level_list_is_not_a_release(jsondir):
    (jsondir / 'r.json').write_text('["just", "a list"]')
    with pytest.raises(release.ReleaseError, match='not a release description'):
        release.load(str(jsondir / 'r.json'))


def test_a_missing_file_is_reported(jsondir):
    with pytest.raises(release.ReleaseError, match='cannot read'):
        release.load(str(jsondir / 'gone.json'))


# --- YAML is gone -----------------------------------------------------------

def test_a_yaml_file_is_refused_with_a_hint(jsondir):
    (jsondir / 'r.yml').write_text('sequence_number: 7\nvendor: example.com\n')
    with pytest.raises(release.ReleaseError, match='YAML is no longer read'):
        release.load(str(jsondir / 'r.yml'))


def test_the_tool_imports_no_yaml_library():
    import subprocess
    import sys
    code = ('import sys; import esp_suit_tool.cli; '
            "print([m for m in sys.modules if 'yaml' in m.lower()])")
    out = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True,
                         cwd=release.__file__.rsplit('/esp_suit_tool/', 1)[0])
    assert out.stdout.strip() == '[]', out.stdout + out.stderr


# --- the summary never overwrites the description ---------------------------

def test_the_description_survives_its_own_build(jsondir):
    """The summary is release.info.json, because release.json is the description."""
    (jsondir / 'r.json').write_text(json.dumps(SPEC, indent=2))
    r = run_cli('create', 'r.json', cwd=jsondir)
    assert r.returncode == 0, r.stderr
    assert json.loads((jsondir / 'r.json').read_text()) == SPEC
    assert json.loads((jsondir / 'r.info.json').read_text())['sequence_number'] == 7
    assert (jsondir / 'r.suit').exists()


def test_an_info_path_that_is_the_description_is_refused(jsondir):
    (jsondir / 'r.json').write_text(json.dumps(SPEC, indent=2))
    r = run_cli('create', 'r.json', '--info', 'r.json', cwd=jsondir)
    assert r.returncode == 2 and 'release description itself' in r.stderr
    assert json.loads((jsondir / 'r.json').read_text()) == SPEC
