#!/usr/bin/env python3
"""Astronomial plugin registration and UID validation."""
from __future__ import annotations

import argparse
import base64
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import sys
import uuid

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from certencr import derive_certificate_key, encrypt_bytes

UID_DOMAIN = b"Astronomial UID v1\0"


def read_uid() -> str:
    if os.name == "nt":
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"Astronomial") as key:
                value, _ = winreg.QueryValueEx(key, "UID")
        except OSError as exc:
            raise SystemExit("HKLM\\Astronomial\\UID is missing; install Astronomial_UID.reg as Administrator") from exc
    else:
        value = os.environ.get("ASTRONOMIAL_UID", "")
        if not value:
            raise SystemExit("Set ASTRONOMIAL_UID on non-Windows systems for testing")
    value = str(value).strip()
    try:
        value = str(uuid.UUID(value))
    except ValueError as exc:
        raise SystemExit("UID must be a UUID") from exc
    return value


def uid_hash(uid: str) -> str:
    return hashlib.sha256(UID_DOMAIN + uid.encode("ascii")).hexdigest()


def keypair() -> tuple[str, str, X25519PrivateKey]:
    private = X25519PrivateKey.generate()
    public = private.public_key()
    priv = base64.b64encode(private.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())).decode()
    pub = base64.b64encode(public.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).decode()
    return priv, pub, private


def read_ini(path: Path, section: str) -> dict[str, str]:
    config = configparser.ConfigParser(); config.read(path)
    return dict(config[section]) if section in config else {}


def write_ini(path: Path, section: str, values: dict[str, str]) -> None:
    config = configparser.ConfigParser(); config[section] = values
    with path.open("w", encoding="utf-8") as stream: config.write(stream)
    try: path.chmod(0o600)
    except OSError: pass


def host_config(root: Path, uid: str) -> tuple[dict[str, str], X25519PrivateKey]:
    path = root / "plugins.ini"; values = read_ini(path, "astronomial")
    if values.get("private_key_b64") and values.get("public_key_b64"):
        private = X25519PrivateKey.from_private_bytes(base64.b64decode(values["private_key_b64"]))
        return values, private
    priv, pub, private = keypair()
    write_ini(path, "astronomial", {"algorithm": "X25519", "uid_hash_algorithm": "SHA-256(Astronomial UID v1\\0 || UID)", "uid_hash": uid_hash(uid), "private_key_b64": priv, "public_key_b64": pub})
    return {"private_key_b64": priv, "public_key_b64": pub}, private


def slug(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9._-]+", "-", value).strip("-._")
    if not result: raise SystemExit("plugin name produces an empty directory name")
    return result[:80]


def register_plugin(args: argparse.Namespace) -> None:
    root = Path(args.root).resolve(); root.mkdir(parents=True, exist_ok=True)
    uid = read_uid()
    expected_hash = uid_hash(uid)
    if args.hash_value and not secrets.compare_digest(args.hash_value.lower(), expected_hash):
        raise SystemExit("UID validation failed: supplied hash does not match HKLM\\Astronomial\\UID")
    _, host_private = host_config(root, uid)
    plugin_dir = root / "plugins" / slug(args.name); plugin_dir.mkdir(parents=True, exist_ok=True)
    plugin_priv_b64, plugin_pub_b64, plugin_private = keypair()
    host_public = host_private.public_key()
    host_public_b64 = base64.b64encode(host_public.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).decode()
    shared = host_private.exchange(X25519PublicKey.from_public_bytes(base64.b64decode(plugin_pub_b64)))
    certificate = {
        "schema": 1,
        "algorithm": "X25519 + HKDF-SHA256 + AES-256-GCM",
        "author": args.author,
        "name": args.name,
        "nonce": args.nonce or secrets.token_urlsafe(24),
        "uid": uid,
        "uid_hash": expected_hash,
        "host_public_key_b64": host_public_b64,
        "plugin_public_key_b64": plugin_pub_b64,
    }
    plaintext = json.dumps(certificate, sort_keys=True, separators=(",", ":")).encode()
    key = derive_certificate_key(shared, uid)
    encrypted = encrypt_bytes(plaintext, key, b"Astronomial Plugin Certificate v1")
    cert_path = plugin_dir / (slug(args.name) + ".acert"); cert_path.write_bytes(encrypted)
    write_ini(plugin_dir / "plugin.ini", "plugin", {"name": args.name, "author": args.author, "algorithm": "X25519", "private_key_b64": plugin_priv_b64, "public_key_b64": plugin_pub_b64, "host_public_key_b64": host_public_b64, "certificate": cert_path.name})
    shutil.copy2(HERE / "certencr.py", plugin_dir / "certencr.py")
    print(f"registered plugin: {plugin_dir}")
    print(f"certificate: {cert_path}")
    print(f"uid hash: {expected_hash}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Astronomial plugin registration")
    sub = parser.add_subparsers(dest="action", required=True)
    reg = sub.add_parser("new-plugin")
    reg.add_argument("--author", required=True); reg.add_argument("--name", required=True); reg.add_argument("--nonce")
    reg.add_argument("--hash", dest="hash_value"); reg.add_argument("--root", default="."); reg.set_defaults(func=register_plugin)
    check = sub.add_parser("validate-uid")
    check.add_argument("--hash", dest="hash_value", required=True); check.set_defaults(func=lambda args: (print("valid UID hash") if secrets.compare_digest(args.hash_value.lower(), uid_hash(read_uid())) else (_ for _ in ()).throw(SystemExit("invalid UID hash"))))
    args = parser.parse_args(); args.func(args); return 0


if __name__ == "__main__": raise SystemExit(main())
