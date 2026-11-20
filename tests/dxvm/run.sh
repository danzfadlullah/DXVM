#!/usr/bin/env bash
# Host-side unit-test harness for the DXVM Mali module.
# Usage: bash tests/dxvm/run.sh <test-name|all>
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="$ROOT_DIR/tests/dxvm"
SRC_DIR="$ROOT_DIR/src"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash tests/dxvm/run.sh <test-name|all>" >&2
  exit 2
fi
TARGET="$1"

CXX="${CXX:-g++}"

# The brief's include flags, plus a stub dir for DXVK's unconditional Windows
# headers and -ffunction-sections so unused Windows-dependent Config functions
# can be dropped at link time (--gc-sections) on a non-Windows host.
CXXFLAGS=(
  -std=c++17
  -ffunction-sections
  -I "$TEST_DIR/stubs"
  -I "$ROOT_DIR/include"
  -I "$ROOT_DIR/include/vulkan/include"
  -I "$ROOT_DIR/include/native"
  -I "$ROOT_DIR/include/native/windows"
  -I "$ROOT_DIR/include/native/directx"
  -I "$SRC_DIR"
)
LDFLAGS=( -Wl,--gc-sections )

MODULE_SOURCES=(
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_config.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_context.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_features.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_hint_flags.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_format_policy.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/bcndecode/bcn_decode.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_state_emulation.cpp"
  "$SRC_DIR/dxvk/dxvm_mali/dxvm_mali_vendor.cpp"
)
CONFIG_SOURCE="$SRC_DIR/util/config/config.cpp"

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

run_one() {
  local name="$1"
  local src="$TEST_DIR/test_${name}.cpp"
  local bin="$BUILD_DIR/test_${name}"

  if [[ ! -f "$src" ]]; then
    echo "FAIL: no such test: $src" >&2
    return 1
  fi

  echo "==> Building test_${name}"
  "$CXX" "${CXXFLAGS[@]}" \
    "$src" "${MODULE_SOURCES[@]}" "$CONFIG_SOURCE" \
    "${LDFLAGS[@]}" -o "$bin"

  echo "==> Running test_${name}"
  "$bin"
  echo "PASS: test_${name}"
}

if [[ "$TARGET" == "all" ]]; then
  shopt -s nullglob
  test_files=("$TEST_DIR"/test_*.cpp)
  shopt -u nullglob

  if [[ ${#test_files[@]} -eq 0 ]]; then
    echo "FAIL: no test_*.cpp files found in $TEST_DIR" >&2
    exit 1
  fi

  for test_file in "${test_files[@]}"; do
    name="$(basename "$test_file" .cpp)"
    name="${name#test_}"
    run_one "$name"
  done
  echo "ALL TESTS PASSED"
else
  run_one "$TARGET"
fi
