#!/usr/bin/env python
#
# Encrypted image generation tool. This tool helps in generating encrypted binary image
# in pre-defined format with assistance of RSA-3072 bit key.
#
# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# Magic Byte is created using command: echo -n "esp_encrypted_img" | sha256sum
esp_enc_img_magic = 0x0788b6cf

GCM_KEY_SIZE = 32
MAGIC_SIZE = 4
ENC_GCM_KEY_SIZE = 384
IV_SIZE = 16
BIN_SIZE_DATA = 4
AUTH_SIZE = 16
RESERVED_HEADER = (512 - (MAGIC_SIZE + ENC_GCM_KEY_SIZE + IV_SIZE + BIN_SIZE_DATA + AUTH_SIZE))


def generate_key_GCM(size: int) -> bytes:
    return os.urandom(int(size))


def generate_IV_GCM() -> bytes:
    return os.urandom(IV_SIZE)


def encrypt_binary(plaintext: bytes, key: bytes, IV: bytes) -> tuple:
    encobj = AESGCM(key)
    ct = encobj.encrypt(IV, plaintext, None)
    return ct[:len(plaintext)], ct[len(plaintext):]


def encrypt(input_file: str, rsa_key_file_name: str, output_file: str) -> None:
    print('Encrypting image ...')
    with open(input_file, 'rb') as image:
        data = image.read()

    with open(rsa_key_file_name, 'rb') as key_file:
        key_data = key_file.read()
        if b"-BEGIN RSA PRIVATE KEY" in key_data or b"-BEGIN PRIVATE KEY" in key_data:
            private_key = serialization.load_pem_private_key(key_data, password=None)
            public_key = private_key.public_key()
        elif b"-BEGIN PUBLIC KEY" in key_data:
            public_key = serialization.load_pem_public_key(key_data)
        else:
            print("Error: Please specify encryption key in PEM format")
            raise SystemExit(1)

    gcm_key = generate_key_GCM(GCM_KEY_SIZE)
    iv = generate_IV_GCM()

    encrypted_gcm_key = public_key.encrypt(gcm_key, padding.PKCS1v15())
    ciphertext, authtag = encrypt_binary(data, gcm_key, iv)

    with open(output_file, 'wb') as image:
        image.write(esp_enc_img_magic.to_bytes(MAGIC_SIZE, 'little'))
        image.write((encrypted_gcm_key))
        image.write((iv))
        image.write(len(ciphertext).to_bytes(BIN_SIZE_DATA, 'little'))
        image.write(authtag)
        image.write(bytearray(RESERVED_HEADER))
        image.write(ciphertext)

    print('Done')


def decrypt_binary(ciphertext: bytes, authTag: bytes, key: bytes, IV: bytes) -> bytes:
    encobj = AESGCM(key)
    plaintext = encobj.decrypt(IV, ciphertext + authTag, None)
    return plaintext


def decrypt(input_file: str, rsa_key: str, output_file: str) -> None:
    print('Decrypting image ...')
    with open(rsa_key, 'rb') as key_file:
        private_key = serialization.load_pem_private_key(key_file.read(), password=None)

    with open(input_file, 'rb') as file:
        recv_magic = file.read(MAGIC_SIZE)
        if(int.from_bytes(recv_magic, 'little') != esp_enc_img_magic):
            print('Error: Magic Verification Failed', file=sys.stderr)
            raise SystemExit(1)
        print('Magic verified successfully')

        encrypted_gcm_key = file.read(ENC_GCM_KEY_SIZE)
        gcm_key = private_key.decrypt(encrypted_gcm_key, padding.PKCS1v15())

        iv = file.read(IV_SIZE)
        bin_size = int.from_bytes(file.read(BIN_SIZE_DATA), 'little')
        auth = file.read(AUTH_SIZE)

        file.read(RESERVED_HEADER)
        enc_bin = file.read(bin_size)

    decrypted_binary = decrypt_binary(enc_bin, auth, gcm_key, iv)

    with open(output_file, 'wb') as file:
        file.write(decrypted_binary)
    print('Done')


def main() -> None:
    parser = argparse.ArgumentParser('Encrypted Image Tool')
    subparsers = parser.add_subparsers(dest='operation', help='run enc_image -h for additional help')
    subparsers.add_parser('encrypt', help='Encrypt an binary')
    subparsers.add_parser('decrypt', help='Decrypt an encrypted image')
    parser.add_argument('input_file')
    parser.add_argument('RSA_key', help='Private key for decryption and Private/Public key for encryption')
    parser.add_argument('output_file_name')

    args = parser.parse_args()

    if(args.operation == 'encrypt'):
        encrypt(args.input_file, args.RSA_key, args.output_file_name)
    if(args.operation == 'decrypt'):
        decrypt(args.input_file, args.RSA_key, args.output_file_name)


if __name__ == '__main__':
    main()
