#!/usr/bin/env python
#
# ESP Delta OTA Ptach Generator Tool. This tool helps in generating the compressed patch file
# using BSDiff and Heatshrink algorithms
#
# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import subprocess
import re

try:
    import detools
except:
    print("Please install 'detools'. Use command `pip install -r tools/requirements.txt`")

# Magic Byte is created using command: echo -n "esp_delta_ota" | sha256sum
esp_delta_ota_magic = 0xfccdde10

MAGIC_SIZE = 4
DIGEST_SIZE = 32
RESERVED_HEADER = 64 - (MAGIC_SIZE + DIGEST_SIZE)

def create_patch(chip: str, base_binary: str, new_binary: str, patch_file_name: str) -> None:
    cmd = "esptool.py --chip " + chip + " image_info " + base_binary
    proc = subprocess.Popen([cmd], stdout=subprocess.PIPE, shell=True)
    (out, err) = proc.communicate()
    x = re.search(b"Validation Hash: ([A-Za-z0-9]+) \(valid\)", out)

    os.system("detools create_patch -c heatshrink " + base_binary + " " + new_binary + " " + patch_file_name)
    patch_file_without_header = "patch_file_temp.bin"
    os.system("mv " + patch_file_name + " " + patch_file_without_header)

    with open(patch_file_name, "wb") as patch_file:
        patch_file.write(esp_delta_ota_magic.to_bytes(MAGIC_SIZE, 'little'))
        patch_file.write(bytes.fromhex(x[1].decode()))
        patch_file.write(bytearray(RESERVED_HEADER))
        with open(patch_file_without_header, "rb") as temp_patch:
            patch_file.write(temp_patch.read())

    os.remove(patch_file_without_header)

def main() -> None:
    parser = argparse.ArgumentParser('Delta OTA Patch Generator Tool')
    parser.add_argument('--chip', help="Target", default="esp32")
    parser.add_argument('--base_binary', help="Path of Base Binary for creating the patch")
    parser.add_argument('--new_binary', help="Path of New Binary for which patch has to be created")
    parser.add_argument('--patch_file_name', help="Patch file path", default="patch.bin")

    args = parser.parse_args()

    create_patch(args.chip, args.base_binary, args.new_binary, args.patch_file_name)


if __name__ == '__main__':
    main()
