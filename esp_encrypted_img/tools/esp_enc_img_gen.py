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
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.backends import default_backend
import hashlib

# Magic Byte is created using command: echo -n "esp_encrypted_img" | sha256sum
esp_enc_img_magic = 0x0788b6cf

GCM_KEY_SIZE = 32

# Header sizes
MAGIC_SIZE = 4
ENC_GCM_KEY_SIZE = 384

KDF_SALT_SIZE = 32
SERVER_PUB_KEY_SIZE = 64
RESERVED_SIZE_ECC = ENC_GCM_KEY_SIZE - (SERVER_PUB_KEY_SIZE + KDF_SALT_SIZE)

IV_SIZE = 16
BIN_SIZE_DATA = 4
AUTH_SIZE = 16

RESERVED_HEADER = (512 - (MAGIC_SIZE + ENC_GCM_KEY_SIZE + IV_SIZE + BIN_SIZE_DATA + AUTH_SIZE))

HMAC_KEY_SIZE = 32

PREDEFINED_SALT = b'\x0e\x21\x60\x64\x2d\xae\x76\xd3\x34\x48\xe4\x3d\x77\x20\x12\x3d' \
    b'\x9f\x3b\x1e\xce\xb8\x8e\x57\x3a\x4e\x8f\x7f\xb9\x4f\xf0\xc8\x69'

ITERATIONS = 2048

def generate_key_GCM(size: int, shared_secret: bytes, random_salt: bytes = None) -> tuple:
    if shared_secret is None:
        return os.urandom(int(size)), None
    else:
        if random_salt is None:
            random_salt = os.urandom(KDF_SALT_SIZE)
        # Perform HKDF key derivation using the random salt
        info = "_esp_enc_img_ecc"
        info_bytes = info.encode('utf-8')
        derived_key = HKDF(
            algorithm=hashes.SHA256(),
            length=size,
            salt=random_salt,
            info=info_bytes,
            backend=default_backend()
        ).derive(shared_secret)
        return derived_key, random_salt


def generate_IV_GCM() -> bytes:
    return os.urandom(IV_SIZE)


def encrypt_binary(plaintext: bytes, key: bytes, IV: bytes) -> tuple:
    encobj = AESGCM(key)
    ct = encobj.encrypt(IV, plaintext, None)
    return ct[:len(plaintext)], ct[len(plaintext):]


def load_rsa_key(key_file_name: str) -> str:
    if key_file_name is None:
        print('No key file provided')
        raise SystemExit(1)
    if not os.path.exists(key_file_name):
        print('Error: Key file does not exist')
        raise SystemExit(1)
    with open(key_file_name, 'rb') as key_file:
        key_data = key_file.read()
        if b"-BEGIN RSA PRIVATE KEY" in key_data or b"-BEGIN PRIVATE KEY" in key_data:
            private_key = serialization.load_pem_private_key(key_data, password=None)
            public_key = private_key.public_key()
        elif b"-BEGIN PUBLIC KEY" in key_data:
            private_key = None
            public_key = serialization.load_pem_public_key(key_data)
        else:
            print("Error: Please specify encryption key in PEM format")
            raise SystemExit(1)
    return public_key


def load_ecc_key(key_file_name: str) -> tuple:
    if key_file_name is None:
        print('No key file provided, generating new keypair')
        _, device_pub_key = generate_hmac_key()
    else:
        with open(key_file_name, 'rb') as key_file:
            device_pub_key = serialization.load_pem_public_key(key_file.read(), default_backend())
    server_priv_key, server_pub_key = generate_random_ecc_keypair()
    shared_secret = perform_ecdh(server_priv_key, device_pub_key)
    return shared_secret, server_pub_key


def encrypt(input_file: str, key_file_name: str, output_file: str, scheme: str) -> None:
    print('Encrypting image ...')
    with open(input_file, 'rb') as image:
        data = image.read()

    iv = generate_IV_GCM()

    if scheme == 'RSA-3072':
        public_key = load_rsa_key(key_file_name)
        gcm_key, _ = generate_key_GCM(GCM_KEY_SIZE, None, None)
        encrypted_gcm_key = public_key.encrypt(gcm_key, padding.PKCS1v15())
    elif scheme == 'ECC-256':
        shared_secret, public_key = load_ecc_key(key_file_name)
        gcm_key, kdf_salt = generate_key_GCM(GCM_KEY_SIZE, shared_secret, None)

    ciphertext, authtag = encrypt_binary(data, gcm_key, iv)

    with open(output_file, 'wb') as image:
        image.write(esp_enc_img_magic.to_bytes(MAGIC_SIZE, 'little'))
        if scheme == 'RSA-3072':
            image.write((encrypted_gcm_key))
        elif scheme == 'ECC-256':
            # Write the raw ECC public key to the file
            public_key_to_write = public_key.public_bytes(
                encoding=serialization.Encoding.X962,
                format=serialization.PublicFormat.UncompressedPoint
            )
            # Public key is 65 bytes, first byte is 0x04
            # Remove the first byte and make the size back to 64
            public_key_to_write = public_key_to_write[1:]
            image.write(public_key_to_write)
            image.write(kdf_salt)
            image.write(bytearray(RESERVED_SIZE_ECC))
        image.write(iv)
        image.write(len(ciphertext).to_bytes(BIN_SIZE_DATA, 'little'))
        image.write(authtag)
        image.write(bytearray(RESERVED_HEADER))
        image.write(ciphertext)
    print('Done')


def decrypt_binary(ciphertext: bytes, authTag: bytes, key: bytes, IV: bytes) -> bytes:
    encobj = AESGCM(key)
    plaintext = encobj.decrypt(IV, ciphertext + authTag, None)
    return plaintext


def decrypt(input_file: str, key_file: str, output_file: str, scheme: str) -> None:
    print('Decrypting image ...')
    if key_file is not None:
        with open(key_file, 'rb') as key_file:
            private_key = serialization.load_pem_private_key(key_file.read(), password=None)
    else:
        print('Error: No key file provided for decryption')
        raise SystemExit(1)

    with open(input_file, 'rb') as file:
        recv_magic = file.read(MAGIC_SIZE)
        if (int.from_bytes(recv_magic, 'little') != esp_enc_img_magic):
            print('Error: Magic Verification Failed', file=sys.stderr)
            raise SystemExit(1)

        if scheme == 'RSA-3072':
            encrypted_gcm_key = file.read(ENC_GCM_KEY_SIZE)
            gcm_key = private_key.decrypt(encrypted_gcm_key, padding.PKCS1v15())
        elif scheme == 'ECC-256':
            server_pub_key = file.read(SERVER_PUB_KEY_SIZE)
            server_pub_key = b'\x04' + server_pub_key

            try:
                server_pub_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), server_pub_key)
            except ValueError:
                print('Error: Invalid server public key format', file=sys.stderr)
                raise SystemExit(1)
            shared_secret = perform_ecdh(private_key, server_pub_key)
            kdf_salt = file.read(KDF_SALT_SIZE)
            gcm_key, _ = generate_key_GCM(GCM_KEY_SIZE, shared_secret, kdf_salt)
            _ = file.read(RESERVED_SIZE_ECC)
        print('Magic verified successfully')

        iv = file.read(IV_SIZE)
        bin_size = int.from_bytes(file.read(BIN_SIZE_DATA), 'little')
        auth = file.read(AUTH_SIZE)
        print('Binary size:', bin_size)
        if scheme == 'RSA-3072':
            file.read(RESERVED_HEADER)
        elif scheme == 'ECC-256':
            file.read(RESERVED_HEADER)
        enc_bin = file.read(bin_size)

    decrypted_binary = decrypt_binary(enc_bin, auth, gcm_key, iv)

    with open(output_file, 'wb') as file:
        file.write(decrypted_binary)
    print('Done')


def generate_hmac_key() -> ec.EllipticCurvePublicKey:
    valid_ecc_key = False

    while not valid_ecc_key:
        # Generate a HMAC key for ECC-256
        hmac_key = os.urandom(HMAC_KEY_SIZE)
        curve = ec.SECP256R1()
        raw_hmac = hashlib.pbkdf2_hmac('sha256', hmac_key, PREDEFINED_SALT, ITERATIONS)
        candidate_scalar = int.from_bytes(raw_hmac, byteorder='big')
        if candidate_scalar == 0:
            print('Candidate scalar is zero, retrying...')
            continue

        private_key = ec.derive_private_key(candidate_scalar, curve, default_backend())
        public_key = private_key.public_key()

        # Check if the private key is valid
        try:
            private_key.private_numbers()
        except ValueError:
            print('Invalid private key, retrying...')
            continue

        print('ECC-256 key generated successfully')
        valid_ecc_key = True
        # Save the public key to a file
        with open('device_pub_key.pem', 'wb') as key_file:
            key_file.write(public_key.public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo
            ))
        # Also save the hmac key to a file
        with open('device_hmac_key.bin', 'wb') as hmac_file:
            hmac_file.write(hmac_key)
    return private_key, public_key


def generate_random_ecc_keypair() -> tuple:
    # Generate a random ECC keypair
    curve = ec.SECP256R1()
    private_key = ec.generate_private_key(curve, default_backend())
    public_key = private_key.public_key()
    print('Server ECC-256 keypair generated successfully')
    return private_key, public_key


def generate_rsa_keypair() -> tuple:
    # Generate a random RSA keypair
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=3072,
        backend=default_backend()
    )
    public_key = private_key.public_key()
    # Save the public key to a file
    with open('rsa_pub_key.pem', 'wb') as key_file:
        key_file.write(public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        ))
    # Save the private key to a file
    with open('rsa_priv_key.pem', 'wb') as key_file:
        key_file.write(private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption()
        ))
    print('Server RSA-3072 keypair generated successfully')
    return private_key, public_key


def perform_ecdh(priv_key, pub_key) -> bytes:
    # Perform ECDH key exchange
    shared_key = priv_key.exchange(ec.ECDH(), pub_key)
    print('ECDH key exchange completed successfully')
    return shared_key


def get_scheme(key_file: str) -> str:
    # Based on the keyfile provided, we can determine the scheme
    if key_file is not None:
        with open(key_file, 'rb') as key_file:
            key_data = key_file.read()
            try:
                # Try reading as private key
                private_key = serialization.load_pem_private_key(key_data, password=None)
                public_key = private_key.public_key()
                # If we can read the private key, check if it is RSA or ECC
                if isinstance(private_key, ec.EllipticCurvePrivateKey):
                    scheme = 'ECC-256'
                elif isinstance(private_key, rsa.RSAPrivateKey):
                    scheme = 'RSA-3072'
                else:
                    print('Error: Unsupported key type')
                    raise SystemExit(1)
            except Exception:
                # If we cannot read the private key, check if it is public key
                try:
                    public_key = serialization.load_pem_public_key(key_data)
                    if isinstance(public_key, ec.EllipticCurvePublicKey):
                        scheme = 'ECC-256'
                    elif isinstance(public_key, rsa.RSAPublicKey):
                        scheme = 'RSA-3072'
                    else:
                        print('Error: Unsupported key type')
                        raise SystemExit(1)
                except Exception:
                    print('Error: Invalid key file format')
                    raise SystemExit(1)
    else:
        scheme = 'ECC-256'
    return scheme


def main() -> None:
    parser = argparse.ArgumentParser('Encrypted Image Tool')
    parser.add_argument('--generate_ecc_key', action='store_true', help='Generate ECC keypair and exit')
    parser.add_argument('--generate_rsa_key', action='store_true', help='Generate RSA keypair and exit')
    subparsers = parser.add_subparsers(dest='operation', help='run enc_image -h for additional help')

    encrypt_parser = subparsers.add_parser('encrypt', help='Encrypt a binary')
    encrypt_parser.add_argument('input_file', help='Input file to encrypt')
    encrypt_parser.add_argument('key_file', help='Public key for encryption (PEM format)')
    encrypt_parser.add_argument('output_file', help='Output file for encrypted image')

    decrypt_parser = subparsers.add_parser('decrypt', help='Decrypt an encrypted image')
    decrypt_parser.add_argument('input_file', help='Input file to decrypt')
    decrypt_parser.add_argument('key_file', help='Private key for decryption')
    decrypt_parser.add_argument('output_file', help='Output file for decrypted binary')

    args = parser.parse_args()

    if args.generate_ecc_key:
        generate_hmac_key()
        generate_random_ecc_keypair()
        print('Key generation completed successfully')
        raise SystemExit(0)

    if args.generate_rsa_key:
        generate_rsa_keypair()
        print('Key generation completed successfully')
        raise SystemExit(0)

    if not args.operation:
        parser.print_help()
        raise SystemExit(1)

    # Get the scheme from the key file
    scheme = get_scheme(args.key_file)

    # Supported schemes will be rsa and ecc
    supported_schemes = ['RSA-3072', 'ECC-256']
    if scheme not in supported_schemes:
        print('Error: Unsupported scheme, supported schemes are:', supported_schemes)
        raise SystemExit(1)

    if (args.operation == 'encrypt'):
        encrypt(args.input_file, args.key_file, args.output_file, scheme)
    elif (args.operation == 'decrypt'):
        decrypt(args.input_file, args.key_file, args.output_file, scheme)
    else:
        print('Error: Invalid operation specified')
        raise SystemExit(1)


if __name__ == '__main__':
    main()
