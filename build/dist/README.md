# Astronomial

Astronomial is a KP astrology toolkit with a native Windows chart calculator and a shared command layer usable from the Telegram bot, CLI, or local web UI. The command-line executable is `astro.exe`; `astronomial.exe` opens the GUI.

## Build

Run `build.bat` on Windows with MSYS2 UCRT64 configured, or run `build.sh` from an MSYS2 UCRT64 shell. The build requires the existing Telegram and Swiss Ephemeris dependencies described in `build.sh`.

The build also clones the official [7-Zip repository](https://github.com/ip7z/7zip) into `deps/7zip` using a shallow clone. To use a mirror or fork, set `7ZIP_REPO_URL` before running the build:

```text
export 7ZIP_REPO_URL=https://github.com/your-account/7zip.git
bash build.sh
```

The build is resumable. `pacman --needed`, repository checks, per-file Swiss Ephemeris compilation, CMake configuration, Telegram library installation, and the final executable copy are skipped when their outputs already exist. If a previous build stopped halfway through, run `bash build.sh` again; only missing or stale steps will run.

## Run modes

```text
astronomial.exe                         Open the Windows chart calculator GUI
astro.exe                               Start Telegram mode; requires ASTRONOMIAL_BOT_TOKEN
astro.exe cli help                      Run a command from the terminal
astro.exe cli lords 123.45              Calculate KP lords
astro.exe cli birth_date ...            Generate a chart
astro.exe plugin                        List registered commands
astro.exe web --port 8787               Start the loopback web UI
astro.exe app --port 8787               Start the web UI and open it in a browser
```

The web server binds to `127.0.0.1` by default. Use `--host` only when you intentionally want another interface exposed.

## Build an `.apf` plugin package

The packer is `tools/apf_pack.py`. It creates a deterministic ZIP-based package with a `manifest.toml` and files under `payload/`.

Unsigned package:

```text
python tools/apf_pack.py pack \
  --plugin-id example.chart \
  --version 1.0.0 \
  --key-number 734927 \
  --capability chart.create \
  --directory example_plugin \
  --output example.chart-1.0.0.apf
```

For a signed package, generate an Ed25519 key pair once. Keep the private key only with the registrar:

```text
openssl genpkey -algorithm ED25519 -out registrar-private.pem
openssl pkey -in registrar-private.pem -pubout -out registrar-public.pem
python tools/apf_pack.py pack ... --signing-key registrar-private.pem --output example.chart-1.0.0.apf
python tools/apf_pack.py verify example.chart-1.0.0.apf --public-key registrar-public.pem
```

The signed bytes are the canonical manifest with the `signature` field omitted. The manifest records the plugin ID, version, capabilities, and `key_number`. The public key can be embedded into Astronomial for offline verification; never distribute the private key.

Package layout:

```text
example.chart-1.0.0.apf
├── manifest.toml
└── payload/
    ├── plugin.dll
    └── config.toml
```

## Plugin commands

Plugins implement `PluginCommands` from `plugin_api.hpp` and return command metadata plus a `run()` implementation. The host registers commands once, then dispatches requests from every interface using `CommandRequest.source` values such as `cli`, `web`, `app`, and `telegram`.

The current source includes the stable API contract and a public key identifier:

```cpp
#define plugin_key astronomial::ASTRONOMIAL_PLUGIN_KEY_ID
```

This identifier is not a secret and is not by itself a license mechanism. The packer now supports Ed25519 signatures, but the host-side `.apf` loader still needs to be wired to verify the manifest before loading plugin code. Do not derive a secret from a number distributed with every application, because that can be reproduced by anyone who has the application.

## X25519 plugin handshake

`tools/dh_exchange.py` is a small OpenSSL-backed Diffie–Hellman helper. Astronomial's host private/public key pair is stored in `./plugins.ini`; each plugin receives a newly generated random X25519 private key and stores its own key configuration separately.

Create the Astronomial host key in the project root:

```text
python tools/dh_exchange.py init-host --output plugins.ini
```

Create a random plugin key and copy the host public key into its configuration:

```text
python tools/dh_exchange.py init-plugin \
  --host-config plugins.ini \
  --output plugin-key.ini
```

Test both sides deriving the same shared secret:

```text
python tools/dh_exchange.py derive \
  --host-config plugins.ini \
  --plugin-config plugin-key.ini
```

The helper prints the shared secret's SHA-256 fingerprint. Do not commit `plugins.ini`, `plugin-key.ini`, or their private keys. Diffie–Hellman provides confidentiality for the handshake, not plugin authenticity; keep the Ed25519 `.apf` signature verification as the authentication mechanism.

## Register a plugin and create an `.acert`

First import `Astronomial_UID.reg` as Administrator after replacing its placeholder UUID. It creates:

```text
HKLM\Astronomial\UID
```

For automatic installation, use `astronomial.iss` as the canonical Inno Setup script (`Astronomial_UID.iss` includes it for compatibility). Its `PrepareToInstall` code preserves a valid existing UID during upgrades and repairs, and calls `CreateGUID` only when the value is missing or malformed. The installer must use `PrivilegesRequired=admin` because the UID and machine environment variables are stored under `HKLM`.

The UID validation value is:

```text
SHA-256(UTF-8("Astronomial UID v1" + NUL + canonical lowercase UUID))
```

Register a plugin from the Astronomial project root:

```text
astro.exe register new plugin \
  --author "Creator Name" \
  --name "Plugin Name" \
  --nonce optional-registration-value
```

This creates or reuses the host X25519 key in `plugins.ini`, generates a fresh random plugin X25519 key, derives a host/plugin shared secret, encrypts the certificate with AES-256-GCM, and creates:

```text
plugins/Plugin-Name/
├── Plugin-Name.acert
├── plugin.ini
└── certencr.py
```

The certificate contains the author, plugin name, nonce, UID hash, host public key, and plugin public key. The encrypted file is authenticated; changing it causes decryption to fail. The UID hash is an identity-consistency check, not a replacement for Ed25519 plugin signatures.

Validate a supplied UID hash:

```text
astro.exe register plugindist --hash <SHA256-UID-HASH>
```

All generated X25519 private keys and AES-GCM nonces use CSPRNG-backed libraries. The host private key and plugin private key must remain private. The current plugin loader still needs to call the certificate decryptor and reject a plugin before loading it when `certencr.py` or `.acert` is missing or invalid.

## Notes

The uploaded archive did not contain a plugin loader, so this change adds the command API and `.apf` packer foundation but does not invent an untested binary plugin loader. The existing astrology calculations and Telegram commands are retained.
