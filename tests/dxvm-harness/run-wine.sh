#!/usr/bin/env bash
# Run the DXVM test harness under Wine against DXVM's DLLs.
#
# Usage:
#   bash tests/dxvm-harness/run-wine.sh [options]
#
# Options:
#   --dll-dir DIR     copy DIR/*.dll beside the harness before running
#   --out PATH        report path (default: dxvm-report.txt)
#   --api LIST        comma list of d3d11,d3d10,d3d9 (default: all)
#   --backend NAME    raw | wrapper | forced (default: raw)
#   --scenes LIST     run only these scenes
#   --show            also stream the report to stdout
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS_DIR="$ROOT_DIR/tests/dxvm-harness"
EXE="$ROOT_DIR/build-win64/tests/dxvm-harness/dxvm-harness.exe"

DLL_DIR=""
OUT="dxvm-report.txt"
API="all"
BACKEND="raw"
SCENES=""
SHOW=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dll-dir)  DLL_DIR="$2"; shift 2 ;;
    --out)      OUT="$2"; shift 2 ;;
    --api)      API="$2"; shift 2 ;;
    --backend)  BACKEND="$2"; shift 2 ;;
    --scenes)   SCENES="$2"; shift 2 ;;
    --show)     SHOW=1; shift ;;
    -h|--help)  sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [[ ! -f "$EXE" ]]; then
  echo "Harness not built: $EXE" >&2
  echo "Build it with:  meson compile -C build-win64 dxvm-harness" >&2
  exit 1
fi

EXE_DIR="$(dirname "$EXE")"

# Windows resolves DLLs from the application directory first, so copying DXVM's
# DLLs here is what makes the harness exercise DXVM instead of any system copy.
if [[ -n "$DLL_DIR" ]]; then
  if [[ ! -d "$DLL_DIR" ]]; then
    echo "No such DLL directory: $DLL_DIR" >&2
    exit 1
  fi
  cp -f "$DLL_DIR"/*.dll "$EXE_DIR"/
  echo "Copied DXVM DLLs from $DLL_DIR"
fi

export WINEDLLOVERRIDES="d3d8,d3d9,d3d10core,d3d11,dxgi=n"
export DXVK_HUD="${DXVK_HUD:-}"

ARGS=(--out "$OUT" --api "$API" --backend "$BACKEND")
if [[ -n "$SCENES" ]]; then
  ARGS+=(--scenes "$SCENES")
fi

cd "$EXE_DIR"

if [[ $SHOW -eq 1 ]]; then
  wine dxvm-harness.exe "${ARGS[@]}"
else
  wine dxvm-harness.exe "${ARGS[@]}" >/dev/null
  echo "Report written to $EXE_DIR/$OUT"
fi

echo "Share the whole report file — it is plain text and paste-ready."