#!/usr/bin/env bash
# Astronomial build script for the MSYS2 UCRT64 environment.
# Run from any directory; paths are resolved relative to this script.
set -Eeuo pipefail
cd "$(dirname "$(realpath "$0")")"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
command -v pacman >/dev/null 2>&1 || fail 'Run this script from the MSYS2 UCRT64 terminal.'
command -v git >/dev/null 2>&1 || fail 'Git is required. Install it with MSYS2 pacman.'

printf '\n==> Installing required MSYS2 packages (missing packages only)\n'
pacman -S --needed --noconfirm \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-curl \
  mingw-w64-ucrt-x86_64-zlib \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  git

for tool in gcc g++ ar cmake ninja git; do
  command -v "$tool" >/dev/null 2>&1 || fail "Required tool '$tool' was not found in PATH. Confirm you opened the MSYS2 UCRT64 terminal."
done

mkdir -p deps
cd deps

SEVENZIP_REPO_URL="${SEVENZIP_REPO_URL:-https://github.com/ip7z/7zip.git}"
if [[ ! -d 7zip/.git ]]; then
  if [[ -e 7zip ]]; then
    fail "deps/7zip exists but is not a Git checkout. Rename/remove it, then rerun."
  fi
  printf '\n==> Fetching 7-Zip source\n'
  git clone --depth 1 "$SEVENZIP_REPO_URL" 7zip
fi

# Swiss Ephemeris
if [[ ! -d swisseph/.git ]]; then
  if [[ -e swisseph ]]; then
    fail "deps/swisseph exists but is not a Git checkout. Rename/remove it, then rerun."
  fi
  printf '\n==> Fetching Swiss Ephemeris\n'
  git clone --depth 1 https://github.com/aloistr/swisseph.git swisseph
fi

printf '\n==> Building Swiss Ephemeris static library\n'
mkdir -p swe-build
swe_sources=(swedate swehouse swejpl swemmoon swemplan sweph swephlib swecl swehel)
for name in "${swe_sources[@]}"; do
  src="swisseph/${name}.c"
  [[ -f "$src" ]] || fail "Expected Swiss Ephemeris source not found: deps/$src"
  if [[ ! -f "swe-build/${name}.o" || "$src" -nt "swe-build/${name}.o" ]]; then
    gcc -O2 -c "$src" -o "swe-build/${name}.o"
  fi
done
ar rcs swe-build/libswe.a swe-build/*.o

# Telegram Bot C++ library, pinned to the release used by this project.
if [[ ! -d tgbot-cpp/.git ]]; then
  if [[ -e tgbot-cpp ]]; then
    fail "deps/tgbot-cpp exists but is not a Git checkout. Rename/remove it, then rerun."
  fi
  printf '\n==> Fetching tgbot-cpp v1.13\n'
  git clone --depth 1 --branch v1.13 https://github.com/reo7sp/tgbot-cpp.git tgbot-cpp
fi

if [[ ! -f tgbot-build/build.ninja && ! -f tgbot-build/Makefile ]]; then
  rm -rf tgbot-build
  cmake -G Ninja -S tgbot-cpp -B tgbot-build \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_CXX_FLAGS='-Wa,-mbig-obj' \
    -DCMAKE_INSTALL_PREFIX="$PWD/tgbot-dist"
fi
printf '\n==> Building tgbot-cpp\n'
cmake --build tgbot-build
if [[ ! -f tgbot-dist/.astronomial-installed ]]; then
  printf '\n==> Installing tgbot-cpp into deps/tgbot-dist\n'
  cmake --install tgbot-build
  touch tgbot-dist/.astronomial-installed
fi
cd ..

# Configure and build Astronomial. If the generated CMake tree is broken,
# preserve it and retry once from a fresh build directory.
printf '\n==> Configuring Astronomial\n'
configure_astronomial() {
  local generator_args=()
  if [[ ! -f build/build.ninja && ! -f build/Makefile ]]; then
    generator_args=(-G Ninja)
  fi
  cmake "${generator_args[@]}" -S . -B build \
    -DCMAKE_CXX_FLAGS='-Wa,-mbig-obj' \
    -DCMAKE_PREFIX_PATH="$PWD/deps/tgbot-dist" \
    -DSWE_DIR="$PWD/deps/swisseph" \
    -DSWE_LIB="$PWD/deps/swe-build/libswe.a"
}

if ! configure_astronomial || ! cmake --build build; then
  printf '\n==> Build tree failed; preserving it and retrying with a clean CMake directory\n' >&2
  recovery_dir="build.recovery-$(date +%Y%m%d-%H%M%S)"
  [[ ! -e "$recovery_dir" ]] || fail "Recovery path already exists: $recovery_dir"
  if [[ -e build ]]; then mv build "$recovery_dir"; fi
  mkdir -p build
  configure_astronomial || fail 'CMake configuration failed after the clean-build retry.'
  cmake --build build || fail "Build failed again. The previous build tree is preserved at $recovery_dir."
fi

cli_exe=build/astro.exe
gui_exe=build/astronomial.exe
[[ -f "$cli_exe" ]] || fail "Build completed but the CLI executable is missing: $cli_exe"
[[ -f "$gui_exe" ]] || fail "Build completed but the GUI executable is missing: $gui_exe"
cp -f "$cli_exe" astro.exe
cp -f "$gui_exe" astronomial.exe
printf '\n==> CLI ready: %s/astro.exe\n' "$PWD"
printf '==> GUI ready: %s/astronomial.exe\n' "$PWD"

mkdir -p ephe
if [[ -d deps/swisseph/ephe ]]; then
  cp -rn deps/swisseph/ephe/. ephe/ 2>/dev/null || true
fi

printf '\nBuild finished successfully.\n'
printf 'The Telegram bot token will be configured automatically by the Inno Setup installer.\n'
printf 'CLI:       ./astro.exe cli help\n'
printf 'Web UI:    ./astro.exe web --port 8787\n'
printf 'Browser:   ./astro.exe app --port 8787\n'
printf 'Plugins:   ./astro.exe plugin\n'