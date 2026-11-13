# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""The esp-suit command line, run the way a user runs it."""
import json
import os
import stat
import subprocess
import sys

from conftest import TOOLS, run_cli

from esp_suit_tool import ids


def test_help_lists_every_command():
    out = run_cli('--help').stdout
    for cmd in ('keygen', 'anchor', 'ids', 'create', 'verify', 'inspect'):
        assert cmd in out


def test_keygen(tmp_path):
    r = run_cli('keygen', tmp_path / 'k.pem', '--anchor', tmp_path / 'a.bin')
    assert r.returncode == 0, r.stderr
    assert stat.S_IMODE(os.stat(tmp_path / 'k.pem').st_mode) == 0o600
    anchor = (tmp_path / 'a.bin').read_bytes()
    assert len(anchor) == 65 and anchor[0] == 0x04
    r = run_cli('keygen', tmp_path / 'k.pem')
    assert r.returncode == 2 and 'exists' in r.stderr
    assert run_cli('keygen', tmp_path / 'k.pem', '--force').returncode == 0


def test_anchor(tmp_path, workdir):
    r = run_cli('anchor', workdir / 'key.pem', '-o', tmp_path / 'a.bin', '--c-array', 'anchor')
    assert r.returncode == 0, r.stderr
    assert 'static const uint8_t anchor[65]' in r.stdout
    assert run_cli('anchor', tmp_path / 'a.bin').stdout.strip() == (tmp_path / 'a.bin').read_bytes().hex()


def test_ids(tmp_path):
    r = run_cli('ids', '--vendor', 'arm.com', '--class', 'x', '-o', tmp_path / 'id.h')
    assert r.returncode == 0, r.stderr
    header = (tmp_path / 'id.h').read_text()
    assert '0xfa, 0x6b, 0x4a, 0x53' in header and 'class_uuid[16]' in header
    r = run_cli('ids', '--pen', '41381', '--class', 'x')
    assert 'vendor_pen = 41381' in r.stdout
    assert run_cli('ids', '--class', 'x').returncode == 2


def test_create_verify_inspect(workdir):
    r = run_cli('create', 'release.yml', cwd=workdir)
    assert r.returncode == 0, r.stderr
    assert (workdir / 'release.suit').exists()
    info = json.loads((workdir / 'release.json').read_text())
    assert info['sequence_number'] == 7 and len(info['components']) == 2

    r = run_cli('verify', 'release.suit', '--key', 'key.pem', '--payload', 'app.bin=app.bin',
                '--vendor', 'espressif.com', '--class', 'tool-test', cwd=workdir)
    assert r.returncode == 0, r.stdout
    assert 'ok    signature' in r.stdout

    r = run_cli('inspect', 'release.suit', cwd=workdir)
    assert r.returncode == 0 and 'directive-try-each' in r.stdout and '"#cfg"' in r.stdout
    r = run_cli('inspect', 'release.suit', '--json', cwd=workdir)
    assert json.loads(r.stdout)['components'][1]['uri'] == '#cfg'


def test_verify_fails_on_a_changed_payload(workdir):
    run_cli('create', 'release.yml', cwd=workdir)
    (workdir / 'bad.bin').write_bytes((workdir / 'app.bin').read_bytes()[:-1])
    r = run_cli('verify', 'release.suit', '--key', 'key.pem', '--payload', 'app.bin=bad.bin', cwd=workdir)
    assert r.returncode == 1 and 'FAIL  payload "app.bin"' in r.stdout


def test_verify_without_a_key_says_so(workdir):
    run_cli('create', 'release.yml', cwd=workdir)
    r = run_cli('verify', 'release.suit', cwd=workdir)
    assert r.returncode == 0 and 'signature was not checked' in r.stdout


def test_create_options(workdir):
    r = run_cli('create', 'release.yml', '-o', 'x.suit', '--sever-install', '--inline-text', '--alg', 'ES256',
                cwd=workdir)
    assert r.returncode == 0, r.stderr
    assert (workdir / 'x.json').exists()
    out = run_cli('inspect', 'x.suit', cwd=workdir).stdout
    assert '/ install / 20: [ -16' in out and 'ES256' in out


def test_errors_exit_2_with_a_message(workdir):
    r = run_cli('create', 'release.yml', '--key', 'missing.pem', cwd=workdir)
    assert r.returncode == 2 and r.stderr.startswith('esp-suit:')
    r = run_cli('create', 'release.yml', '--min-seq', '100', cwd=workdir)
    assert r.returncode == 2 and 'below the floor' in r.stderr


def test_runs_as_a_module(workdir):
    r = subprocess.run([sys.executable, '-m', 'esp_suit_tool', 'ids', '--vendor', 'arm.com', '--class', 'x'],
                       cwd=TOOLS, capture_output=True, text=True)
    assert r.returncode == 0 and str(ids.vendor_uuid('arm.com')) in r.stdout
