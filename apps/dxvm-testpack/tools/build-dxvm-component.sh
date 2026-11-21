#!/usr/bin/env bash
# Build the DXVM component archive PanPlay installs as its DXVK.
#
# Layout (verified against PanPlay's unpacker): tar+zstd, profile.json +
# system32/ + syswow64/ at the root, type "DXVK", and for an arm64ec prefix
# system32 = arm64ec DLLs while syswow64 = i686 DLLs.
#
# Usage: build-dxvm-component.sh [--version V] [--out PATH] [DXVM_REPO]
set -euo pipefail

VERSION="0.1.0-alpha.1-arm64ec"
OUT=""
DXVM_REPO="${DXVM_REPO:-/root/workspace/DXVM/.worktrees/dxvm-clean-arch}"
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLVM_MINGW_VER=20260922
# llvm-mingw ships a host build per CPU; pick the one this host can actually run so the same
# script works on an aarch64 dev box and on x86-64 CI.
case "$(uname -m)" in
  aarch64|arm64) LLVM_MINGW_HOST=aarch64 ;;
  *)             LLVM_MINGW_HOST=x86_64 ;;
esac
LLVM_MINGW_NAME="llvm-mingw-$LLVM_MINGW_VER-ucrt-ubuntu-22.04-$LLVM_MINGW_HOST"
# An already-installed toolchain is reused wherever it is; otherwise it is fetched into a
# writable cache. Never hard-code a developer's absolute path: on a CI runner /root is not even
# readable.
find_llvm_mingw() {
  local base
  for base in "${LLVM_MINGW_DIR:-}" "$HOME/.cache/llvm-mingw/$LLVM_MINGW_NAME" \
              "/root/tc/$LLVM_MINGW_NAME" "${TMPDIR:-/tmp}/$LLVM_MINGW_NAME"; do
    [[ -n "$base" && -x "$base/bin/arm64ec-w64-mingw32-clang" ]] && { printf '%s\n' "$base"; return 0; }
  done
  return 1
}
LLVM_MINGW_DIR="$(find_llvm_mingw || printf '%s\n' "$HOME/.cache/llvm-mingw/$LLVM_MINGW_NAME")"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) VERSION="$2"; shift 2 ;;
    --out)     OUT="$2"; shift 2 ;;
    -h|--help) sed -n '2,12p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) DXVM_REPO="$1"; shift ;;
  esac
done

OUT="${OUT:-${TMPDIR:-/tmp}/dxvm-${VERSION}.wcp}"
DLLS=(d3d10core d3d11 d3d8 d3d9 dxgi)

need() { [[ -e "$1" ]] || { echo "missing: $1" >&2; exit 1; }; }

# llvm-mingw is a 100 MB+ toolchain and is not vendored: fetch the pinned host build on demand.
if [[ ! -x "$LLVM_MINGW_DIR/bin/arm64ec-w64-mingw32-clang" ]]; then
  url="https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VER/$LLVM_MINGW_NAME.tar.xz"
  cache="${LLVM_MINGW_CACHE:-${TMPDIR:-/tmp}}"
  cache="${cache%/}/$LLVM_MINGW_NAME.tar.xz"
  dest="$(dirname "$LLVM_MINGW_DIR")"
  echo "== fetching llvm-mingw $LLVM_MINGW_VER ($LLVM_MINGW_HOST host) into $dest"
  if [[ ! -f "$cache" ]]; then
    curl -fL --retry 3 -o "$cache.part" "$url"
    mv "$cache.part" "$cache"
  fi
  mkdir -p "$dest"
  tar -C "$dest" -xJf "$cache"
  need "$LLVM_MINGW_DIR/bin/arm64ec-w64-mingw32-clang"
fi
echo "== llvm-mingw: $LLVM_MINGW_DIR"

# --- arm64ec (system32) ---
if [[ ! -d "$DXVM_REPO/build-arm64ec" ]]; then
  echo "== configuring arm64ec (this is the slow step)"
  PATH="$LLVM_MINGW_DIR/bin:$PATH" meson setup \
    --cross-file "$DXVM_REPO/build-win-arm64ec.txt" --buildtype release \
    "$DXVM_REPO/build-arm64ec" >/dev/null
fi
echo "== building arm64ec"
PATH="$LLVM_MINGW_DIR/bin:$PATH" meson compile -C "$DXVM_REPO/build-arm64ec" >/dev/null

# --- i686 (syswow64) ---
if [[ ! -d "$DXVM_REPO/build-win32" ]]; then
  meson setup --cross-file "$DXVM_REPO/build-win32.txt" --buildtype release \
    "$DXVM_REPO/build-win32" >/dev/null
fi
echo "== building i686"
meson compile -C "$DXVM_REPO/build-win32" >/dev/null

# --- stage ---
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/system32" "$STAGE/syswow64"

for d in "${DLLS[@]}"; do
  need "$DXVM_REPO/build-arm64ec/src/$([ "$d" = d3d10core ] && echo d3d10 || echo "$d")/$d.dll"
done

cp "$DXVM_REPO/build-arm64ec/src/d3d11/d3d11.dll"     "$STAGE/system32/"
cp "$DXVM_REPO/build-arm64ec/src/d3d9/d3d9.dll"       "$STAGE/system32/"
cp "$DXVM_REPO/build-arm64ec/src/d3d8/d3d8.dll"       "$STAGE/system32/"
cp "$DXVM_REPO/build-arm64ec/src/dxgi/dxgi.dll"       "$STAGE/system32/"
cp "$DXVM_REPO/build-arm64ec/src/d3d10/d3d10core.dll" "$STAGE/system32/"

cp "$DXVM_REPO/build-win32/src/d3d11/d3d11.dll"     "$STAGE/syswow64/"
cp "$DXVM_REPO/build-win32/src/d3d9/d3d9.dll"       "$STAGE/syswow64/"
cp "$DXVM_REPO/build-win32/src/d3d8/d3d8.dll"       "$STAGE/syswow64/"
cp "$DXVM_REPO/build-win32/src/dxgi/dxgi.dll"       "$STAGE/syswow64/"
cp "$DXVM_REPO/build-win32/src/d3d10/d3d10core.dll" "$STAGE/syswow64/"

# Marker: proves the prefix received DXVM and not stock DXVK. Without it a
# silent fallback to PanPlay's own DXVK is invisible in the report.
printf '%s\n' "$VERSION" > "$STAGE/system32/dxvm-marker.txt"

python3 - "$STAGE/profile.json" "$VERSION" <<'PY'
import json, sys
out, version = sys.argv[1], sys.argv[2]
names = ["d3d10core.dll", "d3d11.dll", "d3d8.dll", "d3d9.dll", "dxgi.dll"]
files = []
for n in names:
    files.append({"source": f"system32/{n}", "target": "${system32}/" + n})
for n in names:
    files.append({"source": f"syswow64/{n}", "target": "${syswow64}/" + n})
json.dump({
    "type": "DXVK",
    "versionName": version,
    "versionCode": 1,
    "description": f"DXVM {version} - DXVK 3.1.1 fork for Mali-G615 (Valhall). arm64ec + i686.",
    "files": files,
}, open(out, "w"), indent=2)
PY

# --- pack + verify ---
tar -cf - -C "$STAGE" profile.json system32 syswow64 | zstd -q -19 -o "$OUT"
echo "== wrote $OUT ($(du -h "$OUT" | cut -f1))"

python3 "$TOOLS/verify-component.py" "$OUT"

# --- stage the harness the app runs -----------------------------------------
# The arm64ec build produces dxvm-harness.exe alongside the DLLs. It runs natively on
# arm64ec Wine (no x86_64 emulation), which is why the app's D3D half uses this copy and
# not the i686/x86_64 one. Copying it here keeps the APK asset in step with the DLLs it
# is meant to exercise.
HARNESS_SRC="$DXVM_REPO/build-arm64ec/tests/dxvm-harness/dxvm-harness.exe"
HARNESS_OUT="${HARNESS_OUT:-$TOOLS/../app/src/main/assets/harness/dxvm-harness.exe}"
if [[ -f "$HARNESS_SRC" ]]; then
  mkdir -p "$(dirname "$HARNESS_OUT")"
  cp "$HARNESS_SRC" "$HARNESS_OUT"
  echo "== staged harness $HARNESS_OUT ($(du -h "$HARNESS_OUT" | cut -f1))"
else
  echo "warning: $HARNESS_SRC not found; the app's bundled harness asset is unchanged" >&2
fi
