# Astronomial: build and installer guide (Windows / MSYS2 UCRT64)

## 1. Requirements

- 64-bit Windows
- MSYS2 installed (this project's `build.bat` currently expects `D:\Programmes\msys2`; edit the `MSYS` variable if yours is elsewhere)
- Internet access for MSYS2 packages and GitHub dependency clones
- Inno Setup 6 to create the Windows installer
- Python 3 only if using the `.apf` plugin tools

## 2. Source tree

```text
Astronomial/
├── main.cpp
├── plugin_api.hpp
├── CMakeLists.txt
├── build.bat                 # Windows launcher for build.sh
├── build.sh                  # Installs build packages, fetches deps, compiles app
├── prepare-adddata.bat       # Windows launcher for installer staging
├── prepare-adddata.sh        # Creates adddata/ runtime payload
├── astronomial.iss           # Canonical Inno Setup installer script
├── Astronomial_UID.iss       # Compatibility entry point for the installer
├── Astronomial_UID.reg       # Optional registry template; review before importing
├── BUILDING.md
├── README.md
├── tools/
│   ├── apf_pack.py
│   ├── dh_exchange.py
│   ├── certencr.py
│   └── register_plugin.py
├── deps/                     # Generated/downloaded by build.sh; do not hand-create
│   ├── 7zip/                 # Source checkout used by tooling
│   ├── swisseph/             # Swiss Ephemeris source and upstream ephe data
│   ├── swe-build/            # Compiled .o files and libswe.a
│   ├── tgbot-cpp/            # Telegram library source
│   ├── tgbot-build/          # Telegram library build tree
│   └── tgbot-dist/           # Telegram library install tree
├── build/                    # CMake build output
├── ephe/                     # Runtime ephemeris data copied by build.sh
├── astro.exe                 # Built CLI, Telegram, and web application
├── astronomial.exe           # Built Windows chart calculator GUI
├── adddata/                  # Generated installer payload, made by prepare-adddata.sh
│   ├── astro.exe
│   ├── astronomial.exe
│   ├── *.dll                 # Only runtime DLLs found by ldd, if any
│   ├── tools/certencr.py
│   └── ephe/                 # Ephemeris files installed per user
└── release/                  # Generated installer; created by Inno Setup
```

`deps/`, `build/`, `build.recovery-*/`, `astro.exe`, `astronomial.exe`, `adddata/`, and `release/` are generated artifacts. `build.sh` installs missing MSYS2 build dependencies and downloads source dependencies into `deps/`; do not put the full build directories or `.a` static archives in the installer. If CMake's build tree fails, it preserves the failed tree as `build.recovery-*` and retries once from a clean build directory. `libswe.a` and `tgbot-build/` are needed to compile, not to run the already-linked executables.

## 3. Build the application

1. Open `build.bat` in a text editor. Set `MSYS` to your MSYS2 installation folder if it is not `D:\Programmes\msys2`.
2. Double-click `build.bat`, or open the **MSYS2 UCRT64** terminal, `cd` to the project folder, then run:

   ```bash
   bash build.sh
   ```

3. Wait for package installation, dependency clones, and CMake builds to finish. The script is designed to resume if rerun after a failure.
4. Confirm both `astro.exe` and `astronomial.exe` exist in the project root.

Do not paste a Telegram bot token into source files or commit it. The installer prompts for `ASTRONOMIAL_BOT_TOKEN`; leave it blank to keep the existing machine value. CLI/web/app modes can be tested without a Telegram token, for example:

```bash
./astro.exe cli help
./astro.exe web --port 8787
```

## 4. Stage installer files into `adddata/`

After a successful build, run `prepare-adddata.bat`, or from MSYS2 UCRT64 run:

```bash
bash prepare-adddata.sh
```

This recreates `adddata/`, copies both executables, the available certificate helper, ephemeris data, and runtime DLLs detected by `ldd` from `/ucrt64/bin`. Review the output and verify `adddata/astro.exe`, `adddata/astronomial.exe`, and `adddata/ephe/` contain the expected files before making an installer.

Static `.a` archives and CMake build folders are deliberately not copied to `adddata/`: they are compile-time artifacts and are not normally needed by end users. If runtime testing reveals a missing DLL, add the specific runtime dependency to the staging script and retest on a clean Windows machine.

## 5. Compile the installer

1. Install and open **Inno Setup 6**.
2. Open `astronomial.iss` from the project root. `Astronomial_UID.iss` includes the same script for compatibility.
3. Choose **Build → Compile** (or press F9).
4. The installer is written to `../../release/Astronomial-Setup-V1.0.0-x64.exe`.

The script expects both executables in `adddata/`. It installs the GUI, CLI, and staged runtime DLL dependencies to `{app}`, tools to `{app}\tools`, and ephemeris data to `%LOCALAPPDATA%\.astronomial\ephe`. Runtime dependencies are bundled and installed automatically, so end users do not need MSYS2 or a separate compiler. It adds `{app}` to the machine PATH, broadcasts environment changes, and stores the registry UID under `HKLM\Astronomial\UID`; administrator privileges are required.

## 6. Git tracking recommendations

Commit source files, `CMakeLists.txt`, build/staging scripts, `.iss`, documentation, and any required original data/assets whose redistribution is allowed. Ignore generated build trees, executables, DLLs, static archives, `adddata/`, `release/`, local keys/configs, and secrets. Do not commit Telegram tokens or private signing keys.

## 7. Troubleshooting

- **MSYS2 not found:** edit the `MSYS` path in both `.bat` files.
- **Package errors:** open the MSYS2 UCRT64 terminal and update/install packages, then rerun `bash build.sh`.
- **CMake can't find TgBot:** delete `deps/tgbot-build/` and `deps/tgbot-dist/`, then rerun the build so they are configured again.
- **Installer says source file missing:** build first, then run `prepare-adddata.bat`, and verify both executables exist in `adddata/`.
- **DLL missing on another computer:** `prepare-adddata.sh` collects linked UCRT64 runtime DLLs from both executables. Check `ldd astro.exe` and `ldd astronomial.exe`, stage any additional non-Windows DLL dependencies, then test on a clean Windows installation. The build machine's PATH can mask missing DLLs.
