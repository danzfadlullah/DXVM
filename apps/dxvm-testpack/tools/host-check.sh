#!/usr/bin/env bash
# Everything about this app that can be checked without a phone.
#
# This is the local gate the plan's tasks are verified against in an agent
# environment: component/report logic (pytest), the Vulkan suite's plumbing
# (run-host.sh, host ICD only) and the app's own Kotlin unit tests. APK assembly
# and the real Mali run happen in CI and on the owner's phone — this script
# cannot substitute for either.
#
# Usage: tools/host-check.sh [pytest|kotlin|vulkan|all]   (default: all)
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
WHAT="${1:-all}"
rc=0

step() { echo; echo "=== $* ==="; }

if [[ "$WHAT" == "all" || "$WHAT" == "pytest" ]]; then
  step "component + report tests"
  python3 -m pytest tests/component -q
fi

if [[ "$WHAT" == "all" || "$WHAT" == "vulkan" ]]; then
  step "Vulkan suite plumbing (host ICD — NOT Mali evidence)"
  bash tests/vktests/run-host.sh || rc=$?
fi

if [[ "$WHAT" == "all" || "$WHAT" == "kotlin" ]]; then
  step "Kotlin unit tests"
  export ANDROID_HOME="${ANDROID_HOME:-/root/android-sdk}"
  export ANDROID_SDK_ROOT="$ANDROID_HOME"
  export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-arm64}"
  # AGP ships x86-64-only aapt2 and this host is aarch64. When a local shim that
  # execs it under qemu-user exists, use it; on x86-64 CI or a developer's machine
  # the maven copy is used untouched and this stays empty.
  aapt2_override=()
  [[ -x /usr/local/lib/agp/aapt2 ]] && aapt2_override+=(
    "-Pandroid.aapt2FromMavenOverride=/usr/local/lib/agp/aapt2")
  # -PbundleComponents=false: unit tests do not need the multi-hundred-MB component
  # archives, only the bundled-driver pin (downloaded once into ~/.cache).
  # Drop previous results so the task cannot report UP-TO-DATE: a stale green is
  # indistinguishable from a real pass otherwise.
  rm -rf app/build/test-results app/build/reports/tests
  ./gradlew --no-daemon -Dorg.gradle.jvmargs=-Xmx2500m -PbundleComponents=false \
    "${aapt2_override[@]}" :app:testDebugUnitTest
  python3 - app/build/test-results/testDebugUnitTest <<'PY'
import glob, sys, xml.etree.ElementTree as ET
tests = failures = errors = skipped = 0
for f in glob.glob(sys.argv[1] + "/*.xml"):
    r = ET.parse(f).getroot()
    tests += int(r.get("tests", 0)); failures += int(r.get("failures", 0))
    errors += int(r.get("errors", 0)); skipped += int(r.get("skipped", 0))
print(f"kotlin unit tests: {tests} run, {failures} failed, {errors} errored, {skipped} skipped")
sys.exit(1 if failures or errors or tests == 0 else 0)
PY
fi

exit "$rc"
