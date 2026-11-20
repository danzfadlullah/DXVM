# DXVM Windows Test Harness — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows console harness that reproduces the DXML 1.49 Direct3D test matrix (73 scenes, D3D11/10/9) against DXVM, emitting a report in DXML's own format so the owner can run it under Wine and paste the log back.

**Architecture:** One C++17 console executable, cross-compiled with MinGW-w64. The parent process parses arguments, walks a scene registry, and for each scene spawns itself as a child (`--child <scene>`) so a native crash yields a `CRASH` line instead of truncating the report. Scenes are grouped into focused translation units by concern; a pure report writer formats every line. No D3D code is needed to exercise the report pipeline, so a `--self-test` mode with stub scenes verifies formatting and crash containment on the build host, where Wine is unavailable.

**Tech Stack:** C++17, Meson, MinGW-w64 (x86_64 + i686), Windows `d3d11.h`/`d3d10.h`/`d3d9.h`/`dxgi.h` from `include/native/directx`, `CreateProcess`/`GetExitCodeProcess` for scene isolation.

**Spec:** `docs/superpowers/specs/2026-11-21-dxvm-windows-test-harness-design.md`

## Global Constraints

- C++17, no exceptions across the child-process boundary, no external dependency beyond the Windows SDK headers already vendored in `include/native/directx`.
- Report line kinds are exactly `PASS`, `FAIL`, `SKIP`, `CRASH`; name padded to column 15 with a minimum of one space.
- Device line before results in each suite: `device created, feature level 0xb000` (D3D11) / `10_1` (D3D10) / `device created` (D3D9).
- Section header: `== <name> ==`. Section summary: `SUMMARY <N> passed, <M> failed`.
- Header block: `DXVM Test Pack report, <YYYY-MM-DD HH:MM>`, `Harness: <version> (code <n>)`, `Total: <N> passed, <M> failed or crashed`, blank line, then `Chip:`, `GPU: <name>, driver <ver>`, `Vulkan <ver>`, `DXVM: <dll version>`.
- Diagnostic detail lines are indented by exactly three spaces.
- Every scene must produce exactly one result line per run; a missing line is a harness bug, not a scene failure.
- Non-Mali behaviour of DXVM itself must not change: this plan adds a test target only, and must not touch `src/` or existing DLL exports.
- MSAA reference expectations are pinned from the real DXML run: `msaa2.sampleindex`=9, `msaa4`=26, `msaa8`=60, `msaa16`=128, `msaa*.position` pixel (10,20)=42,82, `msaa4.texture2dms`=128,64,255, `msaa*.*` coverage=64 partial pixels.

## Review Focus

- **Sample-index ramp on Mali.** Stock DXVK 3.1.1 returns 26 for both 2x and 16x where DXML expects 9 and 128; DXVM forks 3.1.1 and likely inherits it. The harness must fail loudly rather than coerce or skip.
- **Missing result lines after a crash.** An access violation mid-scene must still yield a `CRASH` line; a report with a silently absent scene is worse than a failing one.
- **False PASS on a failed device.** If `D3D11CreateDevice` fails, every dependent scene must report `SKIP`/`FAIL` with the HRESULT, never `PASS`.
- **Format drift from DXML.** The report is compared by eye against DXML output; padding, `SUMMARY`, indentation and the totals block must match the observed format, not merely be readable.
- **Wrong-format texture upload.** BCn scenes must distinguish "image was created as ASTC/RGBA8" from "BCn bytes were uploaded raw", which is the failure mode DXVM's transcode path exists to prevent.

---

### Task 1: Harness skeleton, report writer, scene registry

**Files:**
- Create: `tests/dxvm-harness/meson.build`
- Create: `tests/dxvm-harness/src/Scene.h`
- Create: `tests/dxvm-harness/src/Report.h`
- Create: `tests/dxvm-harness/src/Report.cpp`
- Create: `tests/dxvm-harness/src/Registry.h`
- Create: `tests/dxvm-harness/src/Registry.cpp`
- Create: `tests/dxvm-harness/src/main.cpp`
- Create: `tests/dxvm-harness/tests/test_report.cpp`
- Create: `tests/dxvm-harness/tests/run.sh`

**Interfaces:**
- Produces: `enum class SceneStatus { Pass, Fail, Skip, Crash };`
- Produces: `struct SceneResult { SceneStatus status; std::string detail; static SceneResult pass(std::string); static SceneResult fail(std::string); static SceneResult skip(std::string); };`
- Produces: `std::string formatResult(const std::string& name, const SceneResult& r);` returning a line without trailing newline, name padded to column 15 with min 1 space.
- Produces: `std::string formatHeader(const std::string& timestamp, const std::string& harnessVersion, int code, int passed, int failed);`
- Produces: `class Registry` with `void add(const std::string& name, SceneFn fn);`, `const std::vector<Entry>& entries() const;`, `std::vector<Entry> select(const std::string& csv) const;`
- Produces: `std::string formatSummary(int passed, int failed);` returning `SUMMARY <N> passed, <M> failed`.
- Produces: `std::string formatTotals(const std::vector<TotalsRow>& rows);` emitting the
  `== Totals (pass/total) ==` block: one row per group with label padded to column 72,
  then `passed/total` right-aligned in three 11-character columns, matching the real report:

  ```
  == Totals (pass/total) ==
  D3D11                                                                       12/17      12/17      12/17
  ```

- [ ] **Step 1: Write the failing report-format test**

```cpp
// tests/dxvm-harness/tests/test_report.cpp
#include "../src/Report.h"
#include <cstdio>
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main() {
  using namespace dxvmt;
  // Padding: name padded to column 15, minimum one space.
  CHECK(formatResult("depth", SceneResult::pass("left half red")) ==
        "PASS depth          left half red");
  CHECK(formatResult("msaa4.rgba8", SceneResult::pass("64 partial pixels")) ==
        "PASS msaa4.rgba8    64 partial pixels");
  // Long name (>15) still gets exactly one space.
  CHECK(formatResult("msaa16.sampleindex", SceneResult::pass("red is 128, expected 128")) ==
        "PASS msaa16.sampleindex red is 128, expected 128");
  CHECK(formatResult("x", SceneResult::fail("red is 26, expected 9")) ==
        "FAIL x              red is 26, expected 9");
  CHECK(formatResult("x", SceneResult::skip("not ported yet")) ==
        "SKIP x              not ported yet");
  CHECK(formatSummary(90, 2) == "SUMMARY 90 passed, 2 failed");
  std::string h = formatHeader("2026-11-21 04:53", "0.1.0", 1, 314, 0);
  CHECK(h.rfind("DXVM Test Pack report, 2026-11-21 04:53", 0) == 0);
  CHECK(h.find("Total: 314 passed, 0 failed or crashed") != std::string::npos);
  // Totals block: label padded to 72, then three right-aligned passed/total columns.
  std::string t = formatTotals({TotalsRow{"D3D11", 12, 17, 12, 17, 12, 17}});
  CHECK(t.rfind("== Totals (pass/total) ==", 0) == 0);
  CHECK(t.find("D3D11") != std::string::npos);
  CHECK(t.find("12/17") != std::string::npos);
  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_report: OK\n");
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh report`
Expected: FAIL — `Report.h: No such file or directory`.

- [ ] **Step 3: Implement `Report.h`, `Report.cpp`, `Scene.h`**

`formatResult` computes `pad = name.size() >= 15 ? 1 : 15 - name.size()`, then `kind + " " + name + std::string(pad, ' ') + detail`. `formatHeader` emits the three header lines plus a trailing blank line. `formatSummary` is the single `SUMMARY` line. `formatTotals` pads the label to 72
characters and formats each of the three columns as `right-aligned 11-wide passer/total`.

- [ ] **Step 4: Implement `Registry` and a `main.cpp` that lists scenes**

`Registry::add` appends `{name, fn}` in insertion order. `select("")` returns all entries; `select("a,b")` returns entries whose name is in the comma list, preserving registry order, and returns empty for an unknown name so `main` can print `FAIL <name> unknown scene`.

- [ ] **Step 5: Add `--scenes` filtering test**

Add to `test_report.cpp`: build a `Registry`, `add("alpha", ...)`, `add("beta", ...)`, assert `select("")` size 2, `select("beta")` yields one entry named `beta`, `select("nope")` empty.

- [ ] **Step 6: Run the tests to verify they pass**

Run: `bash tests/dxvm-harness/tests/run.sh report`
Expected: `test_report: OK`.

- [ ] **Step 7: Commit**

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): report writer and scene registry"
```

---

### Task 2: Subprocess scene runner and `--self-test`

**Files:**
- Create: `tests/dxvm-harness/src/Child.h`
- Create: `tests/dxvm-harness/src/Child.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`
- Modify: `tests/dxvm-harness/tests/test_report.cpp`
- Create: `tests/dxvm-harness/src/SelfTestScenes.cpp`

**Interfaces:**
- Consumes: `SceneResult`, `Registry` from Task 1.
- Produces: `struct ChildOutcome { bool produced; SceneResult result; int exitCode; };`
- Produces: `ChildOutcome runSceneAsChild(const std::string& exePath, const std::string& sceneName);`
- Produces: `void registerSelfTestScenes(Registry& reg);` registering `selftest.pass`, `selftest.fail`, `selftest.skip`, `selftest.crash` where `crash` dereferences a null pointer.
- Produces: CLI `--self-test` running only those four scenes through the child path.
- Produces: `std::vector<std::pair<std::string,std::string>> parseEnvLines(const std::string& text);`
  parsing `KEY=VALUE` lines, skipping blanks and `#` comments, trimming trailing `\r`.
- Produces: `void applyEnv(const std::vector<std::pair<std::string,std::string>>& pairs);`
  setting each through `_putenv_s`.
- Produces: CLI `--env-file PATH` and repeatable `--set KEY=VALUE`, both applied before device creation.

- [ ] **Step 1: Write the failing crash-containment test**

```cpp
// appended to test_report.cpp: exercise runSceneAsChild against this test binary
// using "--selftest-child <name>" which main() handles before anything else.
CHECK(runSceneAsChild(argv0, "selftest.crash").result.status == SceneStatus::Crash);
CHECK(runSceneAsChild(argv0, "selftest.pass").result.status == SceneStatus::Pass);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh report`
Expected: FAIL — `runSceneAsChild` not defined.

- [ ] **Step 3: Implement `runSceneAsChild`**

Build the command `"<exePath>" --selftest-child <name> > <tmpfile> 2>&1` via `CreateProcessA` with `CREATE_NO_WINDOW`, `WaitForSingleObject(INFINITE)`, `GetExitCodeProcess`. Read the temp file; if it contains a line starting with `PASS `/`FAIL `/`SKIP `, return that as the scene result with `produced = true`. Otherwise return `Crash` with detail `exit <code>`.

- [ ] **Step 4: Implement `--selftest-child` handling in `main`**

Before any other argument handling: if argv contains `--selftest-child`, run the named self-test scene in-process, print exactly one result line, and exit 0 (for pass/fail/skip) or deliberately crash (for `selftest.crash`).

- [ ] **Step 5: Implement the four self-test scenes**

`selftest.pass` → `pass("stub scene ran")`; `selftest.fail` → `fail("stub scene failed on purpose")`; `selftest.skip` → `skip("stub scene skipped")`; `selftest.crash` → `*(volatile int*)nullptr = 1;`.

- [ ] **Step 6: Add and satisfy the env-parsing test**

```cpp
// appended to test_report.cpp
auto p = parseEnvLines("WRAPPER_LOG_LEVEL=info\r\n# comment\n\nDXVK_HUD=dxvm\n");
CHECK(p.size() == 2);
CHECK(p[0].first == "WRAPPER_LOG_LEVEL" && p[0].second == "info");
CHECK(p[1].first == "DXVK_HUD" && p[1].second == "dxvm");
CHECK(parseEnvLines("no_equals_sign\n").empty());
```

Run `bash tests/dxvm-harness/tests/run.sh report`; expect FAIL, implement `parseEnvLines`
(`KEY=VALUE`, skip blanks and `#`, strip trailing `\r`, ignore lines without `=`),
then expect `test_report: OK`.

- [ ] **Step 7: Wire `--self-test` and verify end to end**

Run: `bash tests/dxvm-harness/tests/run.sh report`
Expected: `test_report: OK`, including the crash case reporting `CRASH selftest.crash exit <code>`.

- [ ] **Step 8: Cross-compile check**

Run: `meson setup --cross-file build-win64.txt -Dharness=true build-win64 && meson compile -C build-win64 dxvm-harness`
Expected: `dxvm-harness.exe` produced, exit 0.

- [ ] **Step 9: Commit**

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): subprocess runner, crash containment and env parsing"
```

---

### Task 3: D3D11 device creation and the `device` group

**Files:**
- Create: `tests/dxvm-harness/src/Device.h`
- Create: `tests/dxvm-harness/src/Device.cpp`
- Create: `tests/dxvm-harness/src/scenes/DeviceScenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Consumes: `SceneResult`, `Registry`.
- Produces: `struct D3D11Context { ID3D11Device* device; ID3D11DeviceContext* context; IDXGISwapChain* swapchain; D3D_FEATURE_LEVEL level; };`
- Produces: `bool createD3D11(D3D11Context& out, std::string& error);`
- Produces: `std::string describeGpu();` returning `<name>, driver <ver>` for the report header.
- Produces: `std::string formatDeviceLine(D3D_FEATURE_LEVEL level);` (D3D11). Tasks 9 and 10 add their own
  `formatDeviceLine9()` / `formatDeviceLine10(...)`; these three are distinct functions, not overloads.
- Produces: scenes `device.create`, `device.featurelevels`, `device.formats`.

- [ ] **Step 1: Write the failing device-line test**

Assert `formatDeviceLine(D3D_FEATURE_LEVEL_11_0) == "device created, feature level 0xb000"` and `formatDeviceLine(D3D_FEATURE_LEVEL_10_1) == "device created, feature level 10_1"`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh report`
Expected: FAIL — `formatDeviceLine` not defined.

- [ ] **Step 3: Implement `formatDeviceLine` and `createD3D11`**

`D3D11CreateDevice` with `D3D_DRIVER_TYPE_HARDWARE`, feature levels 11_1 down to 10_0, `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. On failure return the HRESULT in `error` so scenes report `SKIP` with the code rather than `PASS`.

- [ ] **Step 4: Implement the three `device` scenes**

`device.create` → pass with the feature level; `device.featurelevels` → assert the achieved level is ≥ 11_0; `device.formats` → loop a small table of DXGI formats and assert `CheckFormatSupport` returns non-zero for SRV|RT where D3D11 requires it.

- [ ] **Step 5: Print the header device block from `describeGpu`**

Read the adapter description via `IDXGIDevice`/`IDXGIAdapter::GetDesc`; format `GPU: <desc>, driver <vendorId:deviceId>`.

- [ ] **Step 6: Verify**

Run: `bash tests/dxvm-harness/tests/run.sh report && meson compile -C build-win64 dxvm-harness`
Expected: tests OK, exe builds.

- [ ] **Step 7: Commit**

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): D3D11 device creation and device scenes"
```

---

### Task 4: MSAA scenes — highest-value group

**Files:**
- Create: `tests/dxvm-harness/src/scenes/MsaaScenes.cpp`
- Create: `tests/dxvm-harness/src/scenes/Msaa.h`
- Modify: `tests/dxvm-harness/src/main.cpp`
- Create: `tests/dxvm-harness/tests/test_msaa_ramp.cpp`

**Interfaces:**
- Consumes: `D3D11Context`, `SceneResult`.
- Produces: `int expectedSampleIndexRamp(int sampleCount);` returning 9/26/60/128 for 2/4/8/16 and -1 otherwise.
- Produces: scenes `msaa`, `msaa2.sampleindex` … `msaa16.sampleindex`, `msaa*.<fmt>`, `msaa* .position`, `msaa4.texture2dms`, `msaacounts`, `msaashader`.

- [ ] **Step 1: Write the failing ramp test**

```cpp
CHECK(expectedSampleIndexRamp(2) == 9);
CHECK(expectedSampleIndexRamp(4) == 26);
CHECK(expectedSampleIndexRamp(8) == 60);
CHECK(expectedSampleIndexRamp(16) == 128);
CHECK(expectedSampleIndexRamp(3) == -1);
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh msaa_ramp`
Expected: FAIL — not defined.

- [ ] **Step 3: Implement `expectedSampleIndexRamp` with the pinned table**

Constants come from the reference report; do not derive them.

- [ ] **Step 4: Implement `MsaaCounts`**

For each count in {2,4,8,16}: call `CheckMultisampleQualityLevels`, attempt `CreateTexture2D` with that `SampleDesc`, and fail with `CheckMultisampleQualityLevels failed 0x%08x` when the reported count cannot be created. Also fail with `4x MSAA not supported` if 4x is reported unsupported, matching DXML's wording.

- [ ] **Step 5: Implement the per-count sample-index scene**

Render a full-screen triangle whose pixel shader outputs a colour derived from `SV_SampleIndex` scaled to the expected ramp, resolve to a 1-sample target, read pixel (10,20), and compare red against `expectedSampleIndexRamp(count)`. Detail on failure: `red is <got>, expected <want>` — exactly the DXML wording that exposed the upstream bug.

- [ ] **Step 6: Implement `MsaaShader` position, texture2dms and coverage scenes**

`msaa4.position` asserts pixel (10,20) reads `42,82`; `msaa4.texture2dms` asserts `128,64,255`; the `msaa<N>.<fmt>` scenes assert 64 partial-coverage pixels along the diagonal for RGBA8/RGBA16F/BGRA8.

- [ ] **Step 7: Verify**

Run: `bash tests/dxvm-harness/tests/run.sh msaa_ramp && meson compile -C build-win64 dxvm-harness`
Expected: tests OK, exe builds.

- [ ] **Step 8: Commit**

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): MSAA scenes with pinned sample-index ramp"
```

---

### Task 5: State scenes

**Files:**
- Create: `tests/dxvm-harness/src/scenes/StateScenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Consumes: `D3D11Context`, `SceneResult`.
- Produces: scenes `blend`, `blendfactor`, `blendprims`, `depth`, `depthclip`, `depthclamp`, `dualsource`, `logicop`, `scissor`, `viewports`, `wireframe`, `wireframecolor`, `frontface`, `culldistance`, `stencil`, `primitives`, `rendertarget`, `discard`, `discardsemantics`, `errcull`, `errclamp`.

- [ ] **Step 1: Write the failing helper test**

Assert `blendExpectation("alpha")` returns the RGBA tuple documented in the scene table and `logicOpCount() == 10`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh state`
Expected: FAIL.

- [ ] **Step 3: Implement the scenes, one shared render harness**

Each scene creates a small RT, draws a quad with the state under test, reads back one pixel and compares against a constant. `dualsource` must report `N of 10 dual-source blends correct` to match DXML's phrasing. `logicop` reports `N of 10 logic ops correct`.

- [ ] **Step 4: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh state && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): state emulation scenes"
```

---

### Task 6: Texture scenes including BCn

**Files:**
- Create: `tests/dxvm-harness/src/scenes/TextureScenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Produces: scenes `texture`, `texops`, `texarray`, `texture3d`, `cubemap`, `mipmaps`, `formats`, `border`, `gaterhoffset`, `shadowcompare`, `mapnooverwrite`, `rawbuffer`, `typeduav`, and `bcn.<fmt>` for BC1/BC2/BC3/BC4/BC5/BC6H/BC7 with their sRGB variants.

- [ ] **Step 1: Write the failing BCn-format table test**

Assert the scene table contains exactly the 7 BCn base formats plus their sRGB variants, and that `bcnRequiresRemap(VK-like BCn enum)` is true for each.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh texture`
Expected: FAIL.

- [ ] **Step 3: Implement the non-BCn texture scenes**

`texture` asserts all 16 texels of a 4x4 point-sampled texture; `mipmaps` asserts the 1x1 level averages a checkerboard to grey; `formats` asserts bit-exact writes for the eight listed formats.

- [ ] **Step 4: Implement the BCn scenes**

Create a BCn texture with a known block, sample it, resolve and read back. The scene must fail with an explicit message when the image was created with a format different from the requested one **and** the sampled colours do not match the block, distinguishing "remapped correctly" from "raw BCn bytes uploaded".

- [ ] **Step 5: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh texture && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): texture and BCn scenes"
```

---

### Task 7: Compute scenes

**Files:**
- Create: `tests/dxvm-harness/src/scenes/ComputeScenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Produces: scenes `compute`, `atomics`, `append`, `groupshared`, `ballot32`, `ballot32sync`, `dispatchindirect`, `asynccompile`, and the `piece*` family (`piece2bar`, `pieceall`, `pieceatomic`, `piecebits`, `piecedev`, `pieceloop`, `piecepadpred`, `piecepaduncond`, `pieceret`, `pieceshared`, `piecesink`, `pieceslow`, `pieceslow64`, `pieceuav`, `piecewarp0`, `piecewarp3`, `piecewarp6`).

- [ ] **Step 1: Write the failing dispatch-geometry test**

Assert `computeGroupCount(64, 32) == 2` and `computeThreadCount(64) == 64`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh compute`
Expected: FAIL.

- [ ] **Step 3: Implement `compute` and `atomics`**

`compute` dispatches 64 threads each writing their index, then asserts an atomic counter reached 64 and every UAV slot holds its own index. `atomics` exercises InterlockedAdd/Min/Max on a UAV.

- [ ] **Step 4: Implement the `piece*` family as a shared warp-scene driver**

DXML's `scenePiece*` variants differ only in shader shape (loop, shared, atomic, warp count). Implement one `runWarpScene(name, shaderSource)` helper and register each variant with its own HLSL string; a variant that cannot be compiled reports `SKIP` with the compiler error, never `PASS`.

- [ ] **Step 5: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh compute && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): compute and warp scenes"
```

---

### Task 8: Draw scenes

**Files:**
- Create: `tests/dxvm-harness/src/scenes/DrawScenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Produces: scenes `drawindirect`, `instancing`, `geometry`, `tessellation`, `query`, `copy`, `alu`, `internalpasses`, `errstrides`.

- [ ] **Step 1: Write the failing query-expectation test**

Assert `queryExpectation(visible=true) == 4096` and `queryExpectation(false) == 0`, matching the reference report.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh draw`
Expected: FAIL.

- [ ] **Step 3: Implement the scenes**

`instancing` asserts 4 instances with per-instance offset and colour; `geometry` asserts a point expanded to a 32x32 quad; `tessellation` asserts no cracks; `query` asserts 4096 visible samples and 0 hidden; `internalpasses` reports `N of 4 internal pass cases correct`.

- [ ] **Step 4: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh draw && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): draw scenes"
```

---

### Task 9: D3D9 scenes

**Files:**
- Create: `tests/dxvm-harness/src/Device9.h`
- Create: `tests/dxvm-harness/src/Device9.cpp`
- Create: `tests/dxvm-harness/src/scenes/D3D9Scenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Produces: `bool createD3D9(IDirect3DDevice9** out, std::string& error);`
- Produces: scenes `d3d9.device`, `d3d9.msaa`, `d3d9.msaa<count>`, plus the shared scene set rendered through D3D9.

- [ ] **Step 1: Write the failing D3D9 device-line test**

Assert `formatDeviceLine9() == "device created"` and that the D3D9 suite preamble string is `Direct3D 9 scene suite starting`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh d3d9`
Expected: FAIL.

- [ ] **Step 3: Implement `createD3D9` and the suite**

`Direct3DCreate9` → `CreateDevice` with a hidden window, D3DDEVTYPE_HAL, `D3DCREATE_HARDWARE_VERTEXPROCESSING`. Scene names use the `d3d9.` prefix.

- [ ] **Step 4: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh d3d9 && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): Direct3D 9 suite"
```

---

### Task 10: D3D10 scenes

**Files:**
- Create: `tests/dxvm-harness/src/Device10.h`
- Create: `tests/dxvm-harness/src/Device10.cpp`
- Create: `tests/dxvm-harness/src/scenes/D3D10Scenes.cpp`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Produces: `bool createD3D10(ID3D10Device** out, std::string& error);`
- Produces: scenes `d3d10.create`, `d3d10.rendertarget`, `d3d10.texture`, `d3d10.blend`, `d3d10.depth`.

- [ ] **Step 1: Write the failing D3D10 device-line test**

Assert `formatDeviceLine10(D3D10_FEATURE_LEVEL_10_1) == "device created, feature level 10_1"`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh d3d10`
Expected: FAIL.

- [ ] **Step 3: Implement `createD3D10` through `d3d10core` and the five scenes**

- [ ] **Step 4: Verify and commit**

Run: `bash tests/dxvm-harness/tests/run.sh d3d10 && meson compile -C build-win64 dxvm-harness`

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): Direct3D 10 suite"
```

---

### Task 11: Wine launcher, full matrix, report verification

**Files:**
- Create: `tests/dxvm-harness/run-wine.sh`
- Create: `tests/dxvm-harness/README.md`
- Modify: `tests/dxvm-harness/src/main.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: `run-wine.sh` that sets `WINEDLLOVERRIDES=d3d8,d3d9,d3d10core,d3d11,dxgi=n`, copies DXVM DLLs beside the exe when `--dll-dir` is given, runs the harness, and writes `dxvm-report.txt`.
- Produces: `--api d3d11|d3d10|d3d9|all` and `--backend raw|wrapper|forced` run-scope selection.

- [ ] **Step 1: Write the failing run-scope test**

Assert `parseApis("all")` yields 3 entries and `parseApis("d3d11")` yields 1; assert `parseBackend("forced") == Backend::WrapperForced`.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/dxvm-harness/tests/run.sh scope`
Expected: FAIL.

- [ ] **Step 3: Implement run-scope selection and section emission**

`main` emits one `== <api> suite ==` section per selected API and, for stock-comparison runs, one section per backend, matching DXML's block order.

- [ ] **Step 4: Write `run-wine.sh`**

Run: `bash tests/dxvm-harness/run-wine.sh --dll-dir dist/win64 --out dxvm-report.txt`
Expected: script exits 0 and writes a report (verified by the owner on a Wine-capable machine).

- [ ] **Step 5: Write `README.md`**

Document: prerequisites (Wine 9+, DXVM DLLs), how to run, how to read `PASS`/`FAIL`/`SKIP`/`CRASH`, and that `msaa2.sampleindex`/`msaa16.sampleindex` are expected to fail today because DXVM inherits the upstream DXVK 3.1.1 sample-index defect.

- [ ] **Step 6: Full verification**

Run: `bash tests/dxvm-harness/tests/run.sh all && meson compile -C build-win64 dxvm-harness && meson compile -C build-win32 harness`
Expected: all harness unit tests pass, both exes build, `bash tests/dxvm/run.sh all` (the existing DXVM modules) still passes.

- [ ] **Step 7: Commit**

```bash
git add tests/dxvm-harness
git commit -m "feat(harness): Wine launcher and full matrix"
```