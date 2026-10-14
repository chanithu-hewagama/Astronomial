#!/usr/bin/env python3
"""Encrypt and decrypt Astronomial Plugin Certificates (.acert).

The certificate key must be an X25519-derived secret, not a raw UID hash.
AES-GCM supplies confidentiality and tamper detection; X25519 supplies the
shared secret. All nonces and generated keys use cryptography's CSPRNG.
"""
from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path

from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

MAGIC = b"ACERT1\0"


def derive_certificate_key(shared_secret: bytes, uid: str) -> bytes:
    return HKDF(algorithm=SHA256(), length=32, salt=None, info=("Astronomial ACERT v1:" + uid).encode()).derive(shared_secret)


def encrypt_bytes(plaintext: bytes, key: bytes, associated_data: bytes = b"") -> bytes:
    import secrets
    nonce = secrets.token_bytes(12)
    ciphertext = AESGCM(key).encrypt(nonce, plaintext, associated_data)
    return MAGIC + nonce + ciphertext


def decrypt_bytes(blob: bytes, key: bytes, associated_data: bytes = b"") -> bytes:
    if not blob.startswith(MAGIC) or len(blob) < len(MAGIC) + 12 + 16:
        raise ValueError("invalid .acert format")
    nonce = blob[len(MAGIC):len(MAGIC) + 12]
    ciphertext = blob[len(MAGIC) + 12:]
    return AESGCM(key).decrypt(nonce, ciphertext, associated_data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Encrypt/decrypt Astronomial .acert files")
    parser.add_argument("action", choices=["encrypt", "decrypt"])
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--key-b64", required=True)
    parser.add_argument("--uid", required=True)
    args = parser.parse_args()
    key = derive_certificate_key(base64.b64decode(args.key_b64), args.uid)
    data = Path(args.input).read_bytes()
    result = encrypt_bytes(data, key, b"Astronomial Plugin Certificate v1") if args.action == "encrypt" else decrypt_bytes(data, key, b"Astronomial Plugin Certificate v1")
    Path(args.output).write_bytes(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
