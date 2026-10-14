#!/usr/bin/env python3
"""Small X25519 Diffie-Hellman helper for Astronomial/plugin handshakes.

This establishes a shared secret; it does not authenticate a plugin by itself.
Use a signed plugin manifest or an allow-list of public keys for authentication.
Requires OpenSSL 3 on PATH.
"""
from __future__ import annotations

import argparse
import base64
import configparser
from pathlib import Path
import subprocess
import tempfile


def run(*args: str, input: bytes | None = None) -> bytes:
    return subprocess.run(args, input=input, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def make_keypair() -> tuple[bytes, bytes]:
    with tempfile.TemporaryDirectory() as td:
        private = Path(td) / "private.pem"
        public = Path(td) / "public.pem"
        run("openssl", "genpkey", "-algorithm", "X25519", "-out", str(private))
        run("openssl", "pkey", "-in", str(private), "-pubout", "-out", str(public))
        return private.read_bytes(), public.read_bytes()


def write_ini(path: Path, section: str, values: dict[str, str]) -> None:
    config = configparser.ConfigParser(); config[section] = values
    with path.open("w", encoding="utf-8") as stream:
        config.write(stream)
    path.chmod(0o600)


def read_ini(path: Path, section: str) -> dict[str, str]:
    config = configparser.ConfigParser(); config.read(path)
    if section not in config: raise SystemExit(f"missing [{section}] in {path}")
    return dict(config[section])


def pem(value: str) -> bytes:
    return base64.b64decode(value.encode("ascii"))


def b64(value: bytes) -> str:
    return base64.b64encode(value).decode("ascii")


def derive(private_pem: bytes, peer_public_pem: bytes) -> bytes:
    with tempfile.TemporaryDirectory() as td:
        private = Path(td) / "private.pem"; peer = Path(td) / "peer.pem"; shared = Path(td) / "shared.bin"
        private.write_bytes(private_pem); peer.write_bytes(peer_public_pem)
        run("openssl", "pkeyutl", "-derive", "-inkey", str(private), "-peerkey", str(peer), "-out", str(shared))
        return shared.read_bytes()


def init_host(args: argparse.Namespace) -> None:
    private, public = make_keypair()
    write_ini(Path(args.output), "astronomial", {"algorithm": "X25519", "private_key_pem_b64": b64(private), "public_key_pem_b64": b64(public)})
    print(f"created host key configuration: {args.output}")
    print(f"host public key: {b64(public)}")


def init_plugin(args: argparse.Namespace) -> None:
    host = read_ini(Path(args.host_config), "astronomial")
    private, public = make_keypair()
    write_ini(Path(args.output), "plugin", {"algorithm": "X25519", "private_key_pem_b64": b64(private), "public_key_pem_b64": b64(public), "host_public_key_pem_b64": host["public_key_pem_b64"]})
    print(f"created random plugin key configuration: {args.output}")
    print(f"plugin public key: {b64(public)}")


def derive_shared(args: argparse.Namespace) -> None:
    host = read_ini(Path(args.host_config), "astronomial")
    plugin = read_ini(Path(args.plugin_config), "plugin")
    host_secret = derive(pem(host["private_key_pem_b64"]), pem(plugin["public_key_pem_b64"]))
    plugin_secret = derive(pem(plugin["private_key_pem_b64"]), pem(host["public_key_pem_b64"]))
    if host_secret != plugin_secret: raise SystemExit("key agreement failed")
    print(f"shared secret (base64): {b64(host_secret)}")
    print(f"shared secret (sha256): {__import__('hashlib').sha256(host_secret).hexdigest()}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Create and test Astronomial X25519 plugin handshakes")
    sub = parser.add_subparsers(dest="action", required=True)
    host = sub.add_parser("init-host"); host.add_argument("-o", "--output", default="plugins.ini"); host.set_defaults(func=init_host)
    plugin = sub.add_parser("init-plugin"); plugin.add_argument("--host-config", default="plugins.ini"); plugin.add_argument("-o", "--output", default="plugin-key.ini"); plugin.set_defaults(func=init_plugin)
    shared = sub.add_parser("derive"); shared.add_argument("--host-config", default="plugins.ini"); shared.add_argument("--plugin-config", default="plugin-key.ini"); shared.set_defaults(func=derive_shared)
    args = parser.parse_args(); args.func(args); return 0


if __name__ == "__main__": raise SystemExit(main())
