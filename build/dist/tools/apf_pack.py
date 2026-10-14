#!/usr/bin/env python3
"""Pack and verify Astronomial Plugin Files (.apf).

An .apf file is a ZIP archive containing:
  manifest.toml
  payload/<plugin files>

The signed bytes are the canonical manifest with the signature field omitted.
OpenSSL 3 is used for optional Ed25519 signing and verification.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile
import zipfile


def toml_string(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def canonical_manifest(fields: dict[str, object]) -> bytes:
    lines = []
    for key in sorted(fields):
        value = fields[key]
        if isinstance(value, list):
            rendered = "[" + ", ".join(toml_string(str(x)) for x in value) + "]"
        elif isinstance(value, int):
            rendered = str(value)
        else:
            rendered = toml_string(str(value))
        lines.append(f"{key} = {rendered}")
    return ("\n".join(lines) + "\n").encode("utf-8")


def validate_package_path(path: str) -> str:
    clean = PurePosixPath(path.replace("\\", "/"))
    if clean.is_absolute() or ".." in clean.parts or not clean.parts:
        raise ValueError(f"unsafe package path: {path}")
    return str(clean)


def collect_files(directory: Path, files: list[str]) -> list[tuple[Path, str]]:
    result: list[tuple[Path, str]] = []
    if directory:
        for source in sorted(p for p in directory.rglob("*") if p.is_file()):
            result.append((source, "payload/" + source.relative_to(directory).as_posix()))
    for item in files:
        if "=" not in item:
            raise ValueError(f"--file must be source=package/path: {item}")
        source_text, package_path = item.split("=", 1)
        source = Path(source_text).resolve()
        if not source.is_file():
            raise ValueError(f"file does not exist: {source}")
        result.append((source, validate_package_path(package_path)))
    seen = set()
    for _, package_path in result:
        if package_path in seen or package_path == "manifest.toml":
            raise ValueError(f"duplicate package path: {package_path}")
        seen.add(package_path)
    return result


def openssl_sign(key: Path, data: bytes) -> bytes:
    with tempfile.TemporaryDirectory() as td:
        inp, out = Path(td) / "manifest", Path(td) / "signature"
        inp.write_bytes(data)
        subprocess.run(["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", str(key), "-in", str(inp), "-out", str(out)], check=True)
        return out.read_bytes()


def openssl_verify(public_key: Path, data: bytes, signature: bytes) -> bool:
    with tempfile.TemporaryDirectory() as td:
        inp, sig = Path(td) / "manifest", Path(td) / "signature"
        inp.write_bytes(data); sig.write_bytes(signature)
        return subprocess.run(["openssl", "pkeyutl", "-verify", "-rawin", "-pubin", "-inkey", str(public_key), "-in", str(inp), "-sigfile", str(sig)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0


def parse_manifest(text: str) -> tuple[dict[str, object], bytes]:
    fields: dict[str, object] = {}
    signature = b""
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = [x.strip() for x in line.split("=", 1)]
        if key == "signature":
            signature = base64.b64decode(value.strip().strip('"'))
        elif value.startswith("["):
            fields[key] = [x.strip().strip('"') for x in value[1:-1].split(",") if x.strip()]
        elif value.startswith('"'):
            fields[key] = value.strip('"')
        else:
            fields[key] = int(value)
    return fields, signature


def pack(args: argparse.Namespace) -> None:
    files = collect_files(Path(args.directory).resolve() if args.directory else None, args.file)
    fields: dict[str, object] = {
        "capabilities": args.capability,
        "key_number": args.key_number,
        "plugin_id": args.plugin_id,
        "schema": 1,
        "version": args.version,
    }
    canonical = canonical_manifest(fields)
    if args.signing_key:
        fields["signature"] = base64.b64encode(openssl_sign(Path(args.signing_key), canonical)).decode("ascii")
    manifest = canonical_manifest(fields)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        info = zipfile.ZipInfo("manifest.toml", date_time=(1980, 1, 1, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, manifest)
        for source, package_path in sorted(files, key=lambda item: item[1]):
            info = zipfile.ZipInfo(package_path, date_time=(1980, 1, 1, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, source.read_bytes())
    print(f"created {output} ({len(files)} payload files, sha256={hashlib.sha256(output.read_bytes()).hexdigest()})")


def verify(args: argparse.Namespace) -> None:
    with zipfile.ZipFile(args.package) as archive:
        names = archive.namelist()
        if "manifest.toml" not in names:
            raise ValueError("manifest.toml is missing")
        for name in names:
            validate_package_path(name)
        fields, signature = parse_manifest(archive.read("manifest.toml").decode("utf-8"))
        if not signature:
            raise ValueError("package has no signature")
        if not args.public_key:
            raise ValueError("--public-key is required for signed verification")
        if not openssl_verify(Path(args.public_key), canonical_manifest(fields), signature):
            raise ValueError("invalid Ed25519 signature")
    print("valid signature")


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack or verify Astronomial .apf plugin files")
    sub = parser.add_subparsers(dest="action", required=True)
    p = sub.add_parser("pack")
    p.add_argument("--plugin-id", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--key-number", type=int, required=True)
    p.add_argument("--capability", action="append", default=[])
    p.add_argument("--directory")
    p.add_argument("--file", action="append", default=[], help="source=payload/path")
    p.add_argument("--signing-key")
    p.add_argument("-o", "--output", required=True)
    p.set_defaults(func=pack)
    v = sub.add_parser("verify")
    v.add_argument("package")
    v.add_argument("--public-key", required=True)
    v.set_defaults(func=verify)
    args = parser.parse_args()
    try:
        args.func(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
