#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run esp-suit without installing it: python esp_suit.py <command> ..."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from esp_suit_tool.cli import main  # noqa: E402

if __name__ == '__main__':
    sys.exit(main())
