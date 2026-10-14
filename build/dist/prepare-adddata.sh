#!/usr/bin/env bash
# Collect runtime files for the Inno Setup installer. Run from the project root.
set -euo pipefail
cd "$(dirname "$0")"

CLI_EXE="${CLI_EXE:-$PWD/astro.exe}"
GUI_EXE="${GUI_EXE:-$PWD/astronomial.exe}"
[[ -f "$CLI_EXE" ]] || { echo "Missing $CLI_EXE. Run ./build.sh first." >&2; exit 1; }
[[ -f "$GUI_EXE" ]] || { echo "Missing $GUI_EXE. Run ./build.sh first." >&2; exit 1; }

rm -rf adddata
mkdir -p adddata/tools adddata/ephe
cp "$CLI_EXE" adddata/astro.exe
cp "$GUI_EXE" adddata/astronomial.exe
[[ -f tools/certencr.py ]] && cp tools/certencr.py adddata/tools/

# Ephemeris data is runtime data; prefer the project's ephe/ copy, then upstream.
if [[ -d ephe ]] && find ephe -type f -print -quit | grep -q .; then
  cp -R ephe/. adddata/ephe/
elif [[ -d deps/swisseph/ephe ]]; then
  cp -R deps/swisseph/ephe/. adddata/ephe/
else
  echo "Warning: no ephe data found; adddata/ephe will be empty." >&2
fi

# Copy DLL dependencies reported by MinGW ldd that live in the UCRT64 runtime.
# Static archives (.a) and build directories are build-time inputs, not runtime files.
if command -v ldd >/dev/null 2>&1; then
  while IFS= read -r dll; do
    [[ -f "$dll" ]] || continue
    case "$(cygpath -u "$dll" 2>/dev/null || printf '%s' "$dll")" in
      /ucrt64/bin/*) cp -u "$dll" adddata/ ;;
    esac
  done < <({ ldd "$CLI_EXE"; ldd "$GUI_EXE"; } 2>/dev/null | sed -nE 's/.*=>[[:space:]]*([^[:space:]]+).*/\1/p; s/^[[:space:]]*([^[:space:]]+\.dll)[[:space:]]+\(.*/\1/p' | sort -u)
fi

# Copy known UCRT64 runtime DLLs only when present and actually linked by the executable.
# ldd output is the source of truth above; this list is a harmless fallback for common GCC runtimes.
for name in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
  if [[ -f "/ucrt64/bin/$name" ]] && { ldd "$CLI_EXE"; ldd "$GUI_EXE"; } 2>/dev/null | grep -qi "$name"; then
    cp -u "/ucrt64/bin/$name" adddata/
  fi
done

printf '\nInstaller payload staged in %s/adddata\n' "$PWD"
find adddata -maxdepth 2 -type f -printf '%P\n' | sort
