#!/usr/bin/env bash
# Build and run the Vulkan capability suite on the build host.
#
# This verifies the suite's plumbing, not Mali: it runs against whatever ICD the
# host provides (lavapipe = software Vulkan when no GPU is reachable). A FAIL
# here means the *driver under test* lacks the feature — which is the point of
# the suite — and is not a build error.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../../app/src/main/cpp/vktests/vktests.c"
BIN="$(mktemp -d)/vktests"
trap 'rm -rf "$(dirname "$BIN")"' EXIT

CC="${CC:-cc}"
echo "== building vktests (host)"
"$CC" -O1 -std=c11 -Wall -Wextra -o "$BIN" "$SRC" -lvulkan

run_with() {
  local label="$1"; shift
  echo "== $label"
  if [[ -n "${1:-}" ]]; then
    VK_ICD_FILENAMES="$1" "$BIN" || true
  else
    "$BIN" || true
  fi
  echo
}

run_with "default ICD" ""
if [[ -f /usr/share/vulkan/icd.d/lvp_icd.json ]]; then
  run_with "lavapipe (software; NOT Mali evidence)" /usr/share/vulkan/icd.d/lvp_icd.json
fi
echo "NOTE: results describe the host ICD only. Mali numbers come from the phone."
