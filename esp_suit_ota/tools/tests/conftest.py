# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import subprocess
import sys

import pytest

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VECTORS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vectors')
sys.path.insert(0, TOOLS)

from esp_suit_tool import keys  # noqa: E402


@pytest.fixture(scope='session')
def spec_key():
    """The draft's published Appendix B key pair."""
    return keys.load_private(os.path.join(VECTORS, 'spec_private.pem'))


@pytest.fixture(scope='session')
def spec_pub():
    return keys.load_public(os.path.join(VECTORS, 'spec_public.pem'))


@pytest.fixture(scope='session')
def appendix_b():
    return [open(os.path.join(VECTORS, f'appendix_b_example{i}.suit'), 'rb').read() for i in range(6)]


@pytest.fixture
def workdir(tmp_path, spec_key):
    """A release directory: a signing key, two payload files and a release.yml that uses them."""
    keys.write_private(spec_key, str(tmp_path / 'key.pem'))
    (tmp_path / 'app.bin').write_bytes(bytes([0xE9]) + bytes(range(256)) * 8)
    (tmp_path / 'cfg.bin').write_bytes(b'CFG-v1' + bytes(100))
    (tmp_path / 'release.yml').write_text(
        'sequence_number: 7\n'
        'vendor: espressif.com\n'
        'class: tool-test\n'
        'text: { en: { update-description: "tool test" } }\n'
        'components:\n'
        '  - { id: [esp32c5, app], file: app.bin, uri: app.bin, skip_if_installed: true }\n'
        '  - { id: [esp32c5, cfg], file: cfg.bin, uri: "#cfg" }\n'
        'signing: { keys: [key.pem] }\n')
    return tmp_path


def run_cli(*args, cwd=None):
    """esp-suit through the shim, the way a user runs it without installing."""
    return subprocess.run([sys.executable, os.path.join(TOOLS, 'esp_suit.py'), *map(str, args)],
                          cwd=cwd, capture_output=True, text=True)
