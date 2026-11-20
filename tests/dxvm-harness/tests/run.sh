#!/usr/bin/env bash
# Host-side unit tests for the DXVM test harness (Linux build host).
# Usage: bash tests/dxvm-harness/tests/run.sh <test-name|all>
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HARNESS_DIR="$ROOT_DIR/tests/dxvm-harness"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash tests/dxvm-harness/tests/run.sh <test-name|all>" >&2
  exit 2
fi
TARGET="$1"

CXX="${CXX:-g++}"
CXXFLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -ffunction-sections
  -I "$HARNESS_DIR/src"
)
LDFLAGS=( -Wl,--gc-sections )

CORE_SOURCES=(
  "$HARNESS_DIR/src/Report.cpp"
  "$HARNESS_DIR/src/Registry.cpp"
  "$HARNESS_DIR/src/Child.cpp"
  "$HARNESS_DIR/src/SelfTestScenes.cpp"
  "$HARNESS_DIR/src/Device.cpp"
  "$HARNESS_DIR/src/scenes/Msaa.cpp"
  "$HARNESS_DIR/src/scenes/Bcn.cpp"
)

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

run_one() {
  local name="$1"
  local src="$HARNESS_DIR/tests/test_${name}.cpp"
  local bin="$BUILD_DIR/test_${name}"

  if [[ ! -f "$src" ]]; then
    echo "FAIL: no such test: $src" >&2
    return 1
  fi

  echo "==> Building test_${name}"
  "$CXX" "${CXXFLAGS[@]}" "$src" "${CORE_SOURCES[@]}" "${LDFLAGS[@]}" -o "$bin"

  echo "==> Running test_${name}"
  "$bin"
  echo "PASS: test_${name}"
}

if [[ "$TARGET" == "all" ]]; then
  shopt -s nullglob
  test_files=("$HARNESS_DIR"/tests/test_*.cpp)
  shopt -u nullglob

  if [[ ${#test_files[@]} -eq 0 ]]; then
    echo "FAIL: no test_*.cpp files found in $HARNESS_DIR/tests" >&2
    exit 1
  fi

  for f in "${test_files[@]}"; do
    name="$(basename "$f" .cpp)"
    name="${name#test_}"
    run_one "$name"
  done
  echo "ALL TESTS PASSED"
else
  run_one "$TARGET"
fi