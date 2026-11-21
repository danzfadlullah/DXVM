# DXVM TestPack (Android) — Design

**Date:** 2026-11-21
**Status:** Draft for owner approval

## Intent

One Android app that answers a single question about DXVM on a Mali phone:

> With the **phone's own Vulkan driver** (no extra driver installed), does DXVM run
> Direct3D 8/9/10/11 correctly — and where exactly does it fail?

It is built by forking **PanPlay** (engine, driver manager, component system, logs),
adding **PanProbe**'s Vulkan-layer tests pointed at the system ICD, and reporting in
**DXML Test Pack**'s format so results are comparable across rounds and can be pasted back.

## Why

- The owner's goal for DXVM is that Windows games run on the **system driver**, with no
  side-loaded driver such as PanVK. That is the whole reason DXVM exists: the system Mali
  driver lacks BCn and several D3D-required features, and DXVM supplies them.
- PanPlay proves the plumbing works on Mali-G615 — Wine (Proton arm64ec) + FEX + built-in
  X server + DXVK 3.1.1 — and its DXVK ships as a swappable `.wcp` component. DXVM is a
  fork of that same 3.1.1, so it can take that component's place.
- PanPlay bundles PanVK *because* the system driver alone cannot carry DXVK. Whether DXVM
  changes that is exactly the measurement this app must produce, and it can produce it as
  a two-column comparison: `system driver + DXVM` versus `PanVK + DXVK 3.1.1`.
- DXML's report format is already the shared language between owner and agent for
  discussing GPU failures; reusing it keeps every report comparable.

## Goals

1. One APK, one Start button: runs a full compatibility pass and produces one shareable
   text report.
2. Tests **D3D** (DXVM) and **Vulkan** (the driver DXVM sits on) in the same run.
3. Identifies the driver under test in the report header: `system` (ARM proprietary) or
   `PanVK`, plus its version/`driverVersion` string.
4. Reuses DXVM's existing `dxvm-harness.exe` scene matrix (73 names) so the Windows-side
   work already done carries over.
5. Reuses PanProbe's Vulkan tests for the driver-capability half.
6. Works on the system driver with **no extra driver installed**; PanVK is optional and
   used only when the owner selects it for comparison.
7. Report is paste-ready plain text; nothing else is needed to interpret it.

## Non-goals

- Not a game launcher: no library, shortcuts, controller mapping, audio, or store.
- Not a driver distribution channel: the app does not fetch or recommend PanVK.
- Not a benchmark: no scoring, no FPS leaderboards.
- Not a DXVK-upstream comparison tool beyond one optional baseline column.

## Anti-goals

- An app that reports PASS when the device was never created, or that silently skips a
  scene. A missing line is a bug; an explicit `SKIP` is acceptable.
- Any report that omits which driver produced it. A result without driver identity is
  uninterpretable for this project.
- Copying DXML code. It has no license; only its output format and scene list may be
  mirrored, both of which are observable facts.

## Constraints

- **Target: system driver.** Default configuration must use the phone's installed ICD. The
  report must say `driver: system` when it does.
- Android: minSdk 29 for the test app (PanPlay uses 28; PanProbe 29 — 29 wins where the
  Vulkan tests need it), built with NDK 29.0.14206865 and CMake 3.22.1, matching PanPlay.
- Licensing: PanPlay MIT for its own code; the built-in X server under
  `app/src/main/java/com/winlator/` and `app/src/main/cpp/winlator/` is LGPL-2.1 and must
  stay so with notices intact. PanProbe MIT with attribution. DXML: format/idea only.
- DXVM's Windows DLLs must be produced for **arm64ec and i686** to slot into PanPlay's
  component set (`runtimes/dxvk/arm64ec.txt`, `runtimes/dxvk/i686.txt`). Today DXVM only
  builds x86_64/i686 with mingw-w64; arm64ec needs llvm-mingw.
- No behaviour change to DXVM itself in this project. This app tests DXVM; it does not fix it.

## Approaches considered

1. **Fork PanPlay (chosen).** Strip it to a test shell, keep Wine+FEX+X server+driver
   manager+logs+components, swap its DXVK component for DXVM, add a Vulkan tab and a
   D3D tab, and report in DXML format. Gets a working Mali-proven engine for free; the
   real work is stripping, the Vulkan tab, and the arm64ec DXVM build.
2. **Build a launcher from scratch.** Full control, but re-implementing Wine/FEX/X server
   integration is months of work with no new information gained.
3. **Thin companion to an installed PanPlay/Winlator.** Fastest to ship, but depends on an
   app the owner may reconfigure or uninstall, and cannot control the driver selection
   that is the point of the test.

PanPlay is chosen; the other two are recorded because the stripping step (1) is the main
risk, and a fallback to (3) is cheap if stripping proves tangled.

## What the fork keeps, drops and adds

**Drops** (game-launcher surface, not testing): games library and `GamesScreen`, shortcut
manager and launch sheet, controller config/editor/overlay/input, audio and gamepad bridge,
perf recorder and perf report, "what's new", cloud/store screens.

**Keeps**: container/prefix management, `imagefs_bionic.txz` unpacking, Wine component
launch, FEX component, built-in X server (`BuiltinXServer`, `XServerActivity`), driver
manager and driver screen, component download/verify/unpack, session logs, log sharing.

**Adds**:

- `DxvmComponent`: builds/loads DXVM's `.wcp` and installs it into the container in place
  of PanPlay's `dxvk-3.1.1-4-arm64ec.wcp`.
- `SystemDriverProbe`: enumerates the device's Vulkan drivers via
  `vkEnumerateInstanceExtensionProperties`/ICD discovery and reports which one was used.
- **Vulkan tab** (`VulkanTests`): ports of PanProbe's C tests, `bc/`, `blend/`
  (dual-source, logic, MRT), `fill-mode`, `multi-viewport`, `dxvk-reqs`, compiled against
  the vendored Vulkan headers and pointed at the system ICD.
- **D3D tab** (`D3DTests`): runs the existing `dxvm-harness.exe` inside the container and
  captures its report file.
- `TestReport`: merges both halves into one DXML-shaped text report.

## Test matrix

### D3D half — DXVM's existing harness

The 73 scene names already implemented in `tests/dxvm-harness/` are reused unchanged. The
app only runs the exe and reads its `--out` file, so the scene list keeps growing on the
Windows side without touching the app.

### Vulkan half — PanProbe's tests on the system driver

| Port | What it establishes | Why DXVM cares |
|---|---|---|
| `bc/` | whether the driver can sample BCn at all, and correct decode values | decides whether DXVM's remap+transcode is required or wasted |
| `blend/blend_dual` | dual-source blend support | `dxvmMaliEmulateGraphicsState` clamps it; if native, emulation can be dropped |
| `blend/blend_logic` | logic-op support | same |
| `blend/blend_mrt` | MRT writes from one shader | MRT scene in the harness |
| `fill-mode` | polygonMode LINE/POINT support | wireframe clamp |
| `multi-viewport` | viewport count > 1 | viewport clamp |
| `dxvk-reqs` | DXVK's required feature/extension set as the driver reports it | validates `dxvmMaliChainFeatures()` against reality |

Each entry reports `PASS`/`FAIL`/`SKIP` with the same wording rules as the D3D half.

## Report format

Header identifies the run unambiguously, then two sections, then totals:

```
DXVM TestPack report, <YYYY-MM-DD HH:MM>
TestPack: <version> (code <n>)
Driver: system | PanVK <version>, <driverVersion string>
DXVM: <component version>
Total: <N> passed, <M> failed or crashed

<device block: vendor/model, Chip:, GPU:, Android, Build:>

== Direct3D 11 suite (system driver) ==
device created, feature level 0xb000
PASS msaa2.sampleindex  red is 9, expected 9
FAIL bcn.bc7             ...
SKIP compute             not ported yet
SUMMARY ...

== Vulkan capabilities (system driver) ==
PASS vk.bc               BCn sampling supported
FAIL vk.fill-mode        polygonMode LINE rejected
SUMMARY ...

== Totals (pass/total) ==
D3D11   ...   Vulkan   ...
```

Optional comparison mode (`--compare-baseline`) runs the same two halves with PanVK + a
stock DXVK component and appends a second pair of sections, so the owner sees
`system+DXVM` next to `PanVK+DXVK` in one file.

## Building DXVM for PanPlay's component slots

PanPlay's DXVK component is built from `runtimes/dxvk/` with cross files for `arm64ec`
(`arm64ec-w64-mingw32-clang`) and `i686` (`i686-w64-mingw32-clang`), packaged as
`dxvk-3.1.1-4-arm64ec.wcp`. DXVM must produce the same shape:

1. Add `runtimes/dxvm/arm64ec.txt` and `runtimes/dxvm/i686.txt` mirroring PanPlay's cross
   files, using llvm-mingw (`scripts/llvm-mingw.sh` equivalent).
2. Build DXVM's DLLs for arm64ec and i686.
3. Package them into `dxvm-<version>-arm64ec.wcp` in the same layout PanPlay's
   `dxvk-*.wcp` uses (DLLs + profile JSON), which is also the layout already produced for
   the DXVM Windows release, so the packager is shared.
4. The app installs that component into the container under the DXVK component id, so
   Wine loads DXVM's `d3d11.dll`/`d3d9.dll`/`dxgi.dll` instead of upstream DXVK's.

Open question left to the implementation plan: whether arm64ec DXVM needs source changes
(arm64ec has different build macros than x86_64) or builds from the current source as-is.

## Left to the builder

- Which PanPlay files can be deleted outright versus left in place unused; the plan should
  prefer deletion but not at the cost of a broken build.
- Whether the Vulkan tests can run in the same process as the D3D run or need their own
  child process (PanPlay lays out how it isolates wine sessions).
- The exact `.wcp` profile layout PanPlay expects, read from its unpacker rather than
  guessed.
- How to surface test progress on a phone screen with the X server taking the display.

## Verification

- **Unit-testable here**: report formatting and merging, driver-identity labelling, scene
  list completeness, and the Vulkan test harness's pure helpers — all as host C++/JVM tests
  with no device.
- **Requires the owner's phone**: the app actually launching Wine and the Vulkan tests.
- **Requires CI or the owner's machine**: assembling the APK (Android SDK + NDK are not
  available in the agent environment) and the arm64ec DXVM build (llvm-mingw not installed).

This split is stated up front because an APK that has never been built or run is not
evidence of anything, and this project must not repeat that mistake.

## Anti-goal check

Before calling this done, the report from a real phone must show: driver identity, the
D3D halves, the Vulkan halves, and at least one `FAIL` or `PASS` that differs between
`system+DXVM` and `PanVK+DXVK`. A report that cannot distinguish the two drivers has
failed the app's purpose.