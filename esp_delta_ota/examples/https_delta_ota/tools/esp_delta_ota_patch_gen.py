#!/usr/bin/env python
#
# ESP Delta OTA Patch Generator Tool. This tool helps in generating the compressed patch file
# using BSDiff and Heatshrink algorithms
#
# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import subprocess
import re
import tempfile
import hashlib
import sys

try:
    import detools
except:
    print("Please install 'detools'. Use command `pip install -r tools/requirements.txt`")

# Magic Byte is created using command: echo -n "esp_delta_ota" | sha256sum
esp_delta_ota_magic = 0xfccdde10

MAGIC_SIZE = 4 # This is the size of the magic byte
DIGEST_SIZE = 32 # This is the SHA256 of the base binary    
HEADER_SIZE = 64
RESERVED_HEADER = HEADER_SIZE - (MAGIC_SIZE + DIGEST_SIZE) # This is the reserved header size

def calculate_sha256(file_path: str) -> str:
    """Calculate the SHA-256 hash of a file."""
    sha256_hash = hashlib.sha256()
    
    with open(file_path, "rb") as f:
        # Read the file in chunks to avoid memory issues for large files
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    
    # Return the hex representation of the hash
    return sha256_hash.hexdigest()

def create_patch(chip: str, base_binary: str, new_binary: str, patch_file_name: str) -> None:
    cmd = "esptool.py --chip " + chip + " image_info " + base_binary
    proc = subprocess.Popen([cmd], stdout=subprocess.PIPE, shell=True)
    (out, err) = proc.communicate()
    x = re.search(b"Validation Hash: ([A-Za-z0-9]+) \\(valid\\)", out)

    os.system("detools create_patch -c heatshrink " + base_binary + " " + new_binary + " " + patch_file_name)
    patch_file_without_header = "patch_file_temp.bin"
    os.system("mv " + patch_file_name + " " + patch_file_without_header)

    with open(patch_file_without_header, "rb") as temp_patch:
        patch_content = temp_patch.read()

    with open(patch_file_name, "wb") as patch_file:
        patch_file.write(esp_delta_ota_magic.to_bytes(MAGIC_SIZE, 'little'))
        patch_file.write(bytes.fromhex(x[1].decode()))
        patch_file.write(bytearray(RESERVED_HEADER))
        patch_file.write(patch_content)

    os.remove(patch_file_without_header)

    # Verifying the created patch file
    verify_patch(base_binary, patch_file_name, new_binary)

# This API applies the patch file over the base_binary file and generates the binary.new file. Then it compares 
# the hash of new_binary and binary.new, if they are the same then the verification is successful, otherwise it fails.
def verify_patch(base_binary: str, patch_to_verify: str, new_binary: str) -> None:

    with open(patch_to_verify, "rb") as original_file:
        original_file.seek(HEADER_SIZE)  # Move the file pointer 64 bytes ahead
        patch_content = original_file.read()  # Read the rest of the file

    with tempfile.NamedTemporaryFile() as temp_file:
        temp_file.write(patch_content)
        temp_file.flush()

        cmd = "detools apply_patch " + base_binary + " " + temp_file.name + " binary.new"

        try:
            proc = subprocess.Popen([cmd], stdout=subprocess.PIPE, shell = True)
        except:
            print("Failed to execute the command `detools apply_patch`")
            return
            
        (out, err) = proc.communicate()

        if err:
            print("Failed to execute the command `detools apply_patch`")
            return

        sha_of_new_created_binary = calculate_sha256("binary.new")
        sha_of_new_binary = calculate_sha256(new_binary)
        
        if sha_of_new_created_binary == sha_of_new_binary:
            print("Patch file verified successfully")
        else:
            print("Failed to verify the patch")
        os.remove("binary.new")

def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python esp_delta_ota_patch_gen.py create_patch/verify_patch [arguments]")
        sys.exit(1)

    command = sys.argv[1]
    parser = argparse.ArgumentParser('Delta OTA Patch Generator Tool')

    if command == 'create_patch':
        parser.add_argument('--chip', help="Target", default="esp32")
        parser.add_argument('--base_binary', help="Path of Base Binary for creating the patch", required=True)
        parser.add_argument('--new_binary', help="Path of New Binary for which patch has to be created", required=True)
        parser.add_argument('--patch_file_name', help="Patch file path", default="patch.bin")
        args = parser.parse_args(sys.argv[2:])
        create_patch(args.chip, args.base_binary, args.new_binary, args.patch_file_name)
    elif command == 'verify_patch':
        parser.add_argument('--base_binary', help="Path of Base Binary for verifying the patch", required=True)
        parser.add_argument('--patch_file_name', help="Patch file path", required=True)
        parser.add_argument('--new_binary', help="Path of New Binary for verifying the patch", required=True)
        args = parser.parse_args(sys.argv[2:])
        verify_patch(args.base_binary, args.patch_file_name, args.new_binary)
    else:
        print("Invalid command. Use 'create_patch' or 'verify_patch'.")
        sys.exit(1)

if __name__ == '__main__':
    main()
