# DXVM TestPack (Android) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship one Android app that runs DXVM's Direct3D suite and a Vulkan-capability suite against the phone's **system** driver, then writes one DXML-shaped report the owner can paste back.

**Architecture:** Fork PanPlay as the engine (Wine/Proton arm64ec + FEX + built-in X server + component system + session logs). Replace its `type=DXVK` component with DXVM's own arm64ec+i686 build. Add a Vulkan tab that ports PanProbe's C tests and points them at the system ICD, a D3D tab that runs the existing `dxvm-harness.exe` inside the container, and a report layer that merges both halves in DXML's format.

**Tech Stack:** Kotlin + Jetpack Compose (PanPlay's existing stack), C/C++ with NDK 29.0.14206865 for the Vulkan tests, CMake 3.22.1, JUnit for host-side Kotlin tests, llvm-mingw 20260922 for DXVM's arm64ec/i686 DLLs, Meson for DXVM itself.

**Spec:** `docs/superpowers/specs/2026-11-21-dxvm-testpack-android-design.md`

## Global Constraints

- The app's default and headline configuration is the **system driver**. `PanVK` may only appear as an explicitly selected comparison. A report that does not name the driver it used is a defect.
- Component contract, verified against PanPlay's unpacker: archive is tar.xz or tar.zst; `profile.json`, `system32/`, `syswow64/` at the archive root; `profile.json` carries `type` and `versionName` (both sanitised to `[A-Za-z0-9._-]`, ≤ 64 KiB); PanPlay's DXVK slot is `type = "DXVK"`; `files[]` targets use `${system32}/<name>.dll` and `${syswow64}/<name>.dll`.
- For an arm64ec prefix: **`system32/` = arm64ec DLLs, `syswow64/` = i686 DLLs**. Never x86_64/x86.
- `versionName` must be unique per build — reinstalling an existing one fails with "already installed, delete first".
- Report line kinds are exactly `PASS`, `FAIL`, `SKIP`, `CRASH`; name padded to column 15 with a minimum of one space; section header `== <name> ==`; summary `SUMMARY <N> passed, <M> failed`.
- Licensing: PanPlay MIT for its own code; the Winlator-derived X server under `app/src/main/java/com/winlator/` and `app/src/main/cpp/winlator/` stays **LGPL-2.1** with `third_party/winlator/LICENSE`, `NOTICE.md` and `app/src/main/assets/components-NOTICE.txt` intact. PanProbe MIT, attributed in `NOTICE.md`. DXML has no licence: mirror its report format and scene names only, never its code.
- No changes to DXVM's behaviour in this plan. This app tests DXVM; fixes belong to a separate plan.
- **Verification honesty:** Android SDK/NDK are absent from the agent environment, so an APK cannot be assembled or run here. Every task states what is host-verifiable (pure logic, C tests against lavapipe, component layout) versus what needs CI (APK assembly) or the owner's phone (real Mali run). No task may claim device evidence it does not have.

## Review Focus

- **Wrong driver silently tested.** If the app runs the bundled PanVK while the report says `system`, every number is a lie. The driver identity must come from the ICD actually loaded, not from a setting.
- **DXVM not actually installed.** If the component install silently keeps PanPlay's DXVK, the D3D half tests upstream DXVK and the owner gets a clean report about the wrong thing. Prove the DXVM DLLs are the ones in the prefix.
- **A crash taking the whole report.** Wine dying mid-suite must still produce a report naming what ran, mirroring the child-process rule the Windows harness already follows.
- **Blanket SKIPs masquerading as a clean run.** A run where the container never started must not read as `0 failed`.
- **LGPL contamination.** Stripping PanPlay's launcher surface must not delete or re-license the Winlator X server files.

---

### Task 1: Vendor PanPlay and prove the untouched fork builds

**Files:**
- Create: `apps/dxvm-testpack/` (clone of `zenithblue-oss/panplay` at a pinned commit)
- Create: `apps/dxvm-testpack/FORK.md` (upstream commit, licence map, what will be stripped)
- Create: `.github/workflows/testpack.yml`

**Interfaces:**
- Produces: a reproducible APK build target (`./gradlew :app:assembleDebug`) with `ANDROID_HOME`, NDK `29.0.14206865`, CMake `3.22.1`.
- Produces: artifact name `dxvm-testpack-debug.apk` uploaded by CI.

- [ ] **Step 1: Pin the upstream commit and vendor it**

Record the exact commit of `zenithblue-oss/panplay` used, plus the `scripts/components.env` pins it depends on. **Keep `minSdk = 28` and `targetSdk = 28` exactly as PanPlay has them.** PanPlay's own comment states why: `targetSdk >= 29` enables W^X, which blocks `execve` of `wine`/`wineserver` from app data and makes `linker64` fail with "could not exec the wine loader". Raising it would break the engine this app exists to drive. The Vulkan C tests therefore compile against NDK API level 28; anything that genuinely needs an API-29 platform symbol is runtime-guarded and reports `SKIP` on older devices. (An earlier draft of this plan said to raise minSdk to 29 — that was wrong and is recorded in the ledger.) `FORK.md` lists each bundled component, its licence, and which ones this fork keeps (Proton, FEX, imagefs, Vox/WoW64 as needed) versus drops (vkd3d, audio) — and states that PanVK stays available only as an optional comparison.

- [ ] **Step 2: Add the CI workflow**

```yaml
# .github/workflows/testpack.yml
name: testpack
on: [push, workflow_dispatch]
jobs:
  apk:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with: { distribution: temurin, java-version: '17' }
      - name: SDK + NDK
        run: |
          yes | sdkmanager --licenses >/dev/null
          sdkmanager "ndk;29.0.14206865" "cmake;3.22.1" >/dev/null
      - name: Unit tests
        working-directory: apps/dxvm-testpack
        run: ./gradlew :app:testDebugUnitTest
      - name: Assemble
        working-directory: apps/dxvm-testpack
        run: ./gradlew -Dorg.gradle.jvmargs=-Xmx3g :app:assembleDebug
      - uses: actions/upload-artifact@v4
        with:
          name: dxvm-testpack-debug
          path: apps/dxvm-testpack/app/build/outputs/apk/debug/*.apk
```

- [ ] **Step 3: Verify**

Run: push the branch and read the CI log.
Expected: `BUILD SUCCESSFUL`, an APK artifact, and the vendored unit tests green. This is the baseline the fork must never break.

- [ ] **Step 4: Commit**

```bash
git add apps/dxvm-testpack .github/workflows/testpack.yml
git commit -m "build(testpack): vendor PanPlay and add APK CI"
```

---

### Task 2: DXVM component pipeline (arm64ec + i686 → .wcp)

**Files:**
- Create: `apps/dxvm-testpack/tools/build-dxvm-component.sh`
- Create: `apps/dxvm-testpack/tools/verify-component.py`
- Create: `apps/dxvm-testpack/tests/component/test_component_layout.py`
- Reuse: `build-win-arm64ec.txt` (already added at the DXVM root and verified)

**Interfaces:**
- Produces: `dxvm-<version>-arm64ec.wcp` — tar+zstd with `profile.json`, `system32/` (arm64ec), `syswow64/` (i686).
- Produces: `verify-component.py <wcp>` exiting non-zero unless: archive is tar.zst; `profile.json` at root with `type=="DXVK"` and unique non-empty `versionName`; every `files[]` source exists; `system32/*.dll` are ARM64EC; `syswow64/*.dll` are i686; `profile.json` ≤ 64 KiB.

- [ ] **Step 1: Write the failing layout test**

```python
# apps/dxvm-testpack/tests/component/test_component_layout.py
import subprocess, sys, pathlib
HERE = pathlib.Path(__file__).resolve().parents[2]

def test_rejects_bad_type(tmp_path):
    bad = tmp_path / "bad.wcp"
    subprocess.run(["python3", str(HERE / "tools/make-fixture-wcp.py"),
                    "--type", "NotDXVK", "--out", str(bad)], check=True)
    r = subprocess.run([sys.executable, str(HERE / "tools/verify-component.py"), str(bad)],
                       capture_output=True, text=True)
    assert r.returncode != 0
    assert "type" in r.stdout.lower() + r.stderr.lower()

def test_accepts_good_component():
    good = pathlib.Path("/root/workspace/dxvm-0.1.0-alpha.1-arm64ec.wcp")
    if not good.exists():
        import pytest; pytest.skip("component not built in this environment")
    r = subprocess.run([sys.executable, str(HERE / "tools/verify-component.py"), str(good)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd apps/dxvm-testpack && python3 -m pytest tests/component -q`
Expected: FAIL — `tools/verify-component.py` missing.

- [ ] **Step 3: Implement `verify-component.py` and the fixture generator**

`verify-component.py` decompresses with zstd, lists the tar, and asserts each rule from the Interfaces block, printing one line per check. PE machine type is read at offset `0x3c → +4`; ARM64EC is machine `0x8664` **with** an `arm64x` marker present in the file (this is the layout PanPlay's own component uses and the one DXVM's arm64ec build produces); i686 is `0x14c`.

- [ ] **Step 4: Implement `build-dxvm-component.sh`**

Steps: fetch/verify llvm-mingw `20260922` (`aarch64` host build — confirmed available), `meson setup --cross-file build-win-arm64ec.txt build-arm64ec` + compile, `meson setup --cross-file build-win32.txt` (i686 via mingw-w64 gcc) + compile, stage `system32/` from the arm64ec build and `syswow64/` from the i686 build, write `profile.json` with `type: "DXVK"` and a version-scoped `versionName`, tar+zstd to `dxvm-<version>-arm64ec.wcp`, then run `verify-component.py` on the result and fail the script if it does not exit 0.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd apps/dxvm-testpack && python3 -m pytest tests/component -q`
Expected: the reject-case and accept-case both pass.

- [ ] **Step 6: Commit**

```bash
git add apps/dxvm-testpack/tools apps/dxvm-testpack/tests/component
git commit -m "feat(testpack): DXVM component build and layout verification"
```

---

### Task 3: Strip the launcher surface

**Files:**
- Delete: game/launcher screens and helpers under `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/` (games library, shortcuts, launch sheet, controller config/editor/overlay/input, audio, gamepad bridge, perf recorder/report, what's-new, store/cloud)
- Modify: navigation entry point so the app opens on the test screen
- Keep untouched: `app/src/main/java/com/winlator/**`, `app/src/main/cpp/winlator/**`, licence notices

**Interfaces:**
- Produces: an app whose only screens are `TestScreen`, `ComponentsScreen`, `DriversScreen`, `LogsScreen`.
- Consumes: PanPlay's container/prefix, component install, driver manager, session logs.

- [ ] **Step 1: Record the licence-preservation check as a test**

Add `apps/dxvm-testpack/tests/component/test_licences.py` asserting these paths still exist and are non-empty: `third_party/winlator/LICENSE`, `NOTICE.md`, `app/src/main/assets/components-NOTICE.txt`, and that `app/src/main/java/com/winlator/XServer.java` (or the X server file actually present) is still there.

- [ ] **Step 2: Run it to verify it passes before stripping**

Run: `cd apps/dxvm-testpack && python3 -m pytest tests/component/test_licences.py -q`
Expected: PASS. This is the guard, so it must be green before deletion starts.

- [ ] **Step 3: Delete launcher files, in dependency order**

Remove leaf screens first (games, shortcuts, controller editor, audio, perf), then their helpers, compiling between groups. Do not touch anything under `com/winlator/`, the X server glue, container code, or component code. Where a kept file references a deleted one, delete the reference rather than the kept file.

- [ ] **Step 4: Verify**

Run: `cd apps/dxvm-testpack && python3 -m pytest tests/component -q && ./gradlew :app:compileDebugKotlin`
Expected: licence test still PASS; Kotlin compiles. In CI, `:app:assembleDebug` still produces an APK.

- [ ] **Step 5: Commit**

```bash
git add -A apps/dxvm-testpack
git commit -m "refactor(testpack): strip launcher surface to a test shell"
```

---

### Task 4: Install DXVM as the DXVK component

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/DxvmComponent.kt`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/DxvmComponentTest.kt`
- Modify: component install/copy path so the prefix receives DXVM's DLLs

**Interfaces:**
- Produces: `DxvmComponent.installedVersion(ctx): String?` and `DxvmComponent.provesDxvmInPrefix(prefixDir: File): Boolean`.
- Produces: `DxvmComponent.install(ctx, wcp: File): Result<Unit>` validating `type=="DXVK"` before handing off to PanPlay's existing installer.

- [ ] **Step 1: Write the failing test**

```kotlin
// DxvmComponentTest.kt
class DxvmComponentTest {
    @Test fun rejectsNonDxvkType() {
        val r = DxvmComponent.validateProfile("""{"type":"VKD3D","versionName":"x"}""")
        assertTrue(r.isFailure)
    }
    @Test fun acceptsDxvkType() {
        assertTrue(DxvmComponent.validateProfile("""{"type":"DXVK","versionName":"0.1.0"}""").isSuccess)
    }
    @Test fun detectsDxvmByMarker() {
        val dir = createTempDir()
        File(dir, "system32").mkdirs()
        File(dir, "system32/d3d11.dll").writeBytes(ByteArray(4))
        assertFalse(DxvmComponent.provesDxvmInPrefix(dir))
        File(dir, "system32/dxvm-marker.txt").writeText("DXVM")
        assertTrue(DxvmComponent.provesDxvmInPrefix(dir))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd apps/dxvm-testpack && ./gradlew :app:testDebugUnitTest --tests '*DxvmComponentTest*'`
Expected: FAIL — unresolved reference `DxvmComponent`.

- [ ] **Step 3: Implement `DxvmComponent`**

`validateProfile` parses the JSON and requires `type == "DXVK"` plus a non-empty `versionName`. `install` delegates to PanPlay's installer after validation. `provesDxvmInPrefix` checks for a marker file the packager writes (`system32/dxvm-marker.txt` containing the component version) **and** that `system32/d3d11.dll` exists — this is the anti-goal guard: without it a silent fallback to upstream DXVK is invisible.

- [ ] **Step 4: Make the packager write the marker**

Update `build-dxvm-component.sh` to write `system32/dxvm-marker.txt` with the version string, and `verify-component.py` to require it.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd apps/dxvm-testpack && ./gradlew :app:testDebugUnitTest --tests '*DxvmComponentTest*' && python3 -m pytest tests/component -q`
Expected: both green.

- [ ] **Step 6: Commit**

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): install DXVM as the DXVK component with proof marker"
```

---

### Task 5: Driver identity probe

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/DriverIdentity.kt`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/DriverIdentityTest.kt`

**Interfaces:**
- Produces: `data class DriverIdentity(val kind: Kind, val name: String, val version: String)` with `enum class Kind { System, PanVk, Unknown }`.
- Produces: `fun classify(vendorId: Int, driverId: String, driverName: String): DriverIdentity`.
- Produces: `fun reportLine(identity: DriverIdentity): String` → `Driver: system, <name> <version>` or `Driver: PanVK <version>`.

- [ ] **Step 1: Write the failing test**

```kotlin
class DriverIdentityTest {
    @Test fun armProprietaryIsSystem() {
        val id = classify(0x13b5, "0x13b5", "Mali-G615 MC2, driver r44p1")
        assertEquals(DriverIdentity.Kind.System, id.kind)
        assertEquals("Driver: system, Mali-G615 MC2, driver r44p1", reportLine(id))
    }
    @Test fun mesaPanvkIsPanVk() {
        val id = classify(0x10005, "VK_DRIVER_ID_MESA_PANVK", "panvk")
        assertEquals(DriverIdentity.Kind.PanVk, id.kind)
        assertTrue(reportLine(id).startsWith("Driver: PanVK"))
    }
    @Test fun unknownIsNeverSystem() {
        assertEquals(DriverIdentity.Kind.Unknown, classify(0, "?", "?").kind)
        assertTrue(reportLine(classify(0, "?", "?")).startsWith("Driver: unknown"))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*DriverIdentityTest*'`
Expected: FAIL — unresolved `classify`.

- [ ] **Step 3: Implement classification and the native read**

`classify` maps ARM vendor `0x13b5` to `System`, `VK_DRIVER_ID_MESA_PANVK` (or a `panvk` driver name) to `PanVk`, everything else to `Unknown`. `Unknown` must never be reported as `system`. A small JNI helper reads the loaded device's `VkPhysicalDeviceProperties` (vendorID, deviceName, `driverVersion` decoded per Vulkan's vendor-specific layout) and feeds `classify`.

- [ ] **Step 4: Verify and commit**

Run: `./gradlew :app:testDebugUnitTest --tests '*DriverIdentityTest*'`
Expected: PASS.

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): identify the driver actually under test"
```

---

### Task 6: Vulkan capability suite (PanProbe ports, system ICD)

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/cpp/vktests/` (ported from PanProbe `tests/dxvk/vulkan/`, MIT, attribution in `NOTICE.md`)
- Create: `apps/dxvm-testpack/app/src/main/cpp/vktests/vktests_jni.c`
- Create: `apps/dxvm-testpack/tests/vktests/run-host.sh`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/VkResultParseTest.kt`

**Interfaces:**
- Produces: one C entry point per suite: `int vk_test_bc(void)`, `vk_test_blend_dual`, `vk_test_blend_logic`, `vk_test_blend_mrt`, `vk_test_fill_mode`, `vk_test_multi_viewport`, `vk_test_dxvk_reqs`, each returning `0` pass, `1` fail, `2` skip, and writing a one-line detail into a caller buffer.
- Produces: JNI `VkTests.run(name: String): String` returning a DXML-shaped result line.

- [ ] **Step 1: Write the failing parse test**

```kotlin
class VkResultParseTest {
    @Test fun mapsReturnCodesToKinds() {
        assertEquals("PASS vk.bc  BCn sampling supported", VkTests.format("vk.bc", 0, "BCn sampling supported"))
        assertEquals("FAIL vk.fill-mode  polygonMode LINE rejected", VkTests.format("vk.fill-mode", 1, "polygonMode LINE rejected"))
        assertEquals("SKIP vk.bc  no BCn formats", VkTests.format("vk.bc", 2, "no BCn formats"))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*VkResultParseTest*'`
Expected: FAIL — unresolved `VkTests`.

- [ ] **Step 3: Port the tests**

Copy PanProbe's `tests/dxvk/vulkan/` sources for the seven suites, keeping their shaders (`.comp`/`.vert`/`.frag` and generated `*_spv.h`). Add a shared bootstrap that creates a `VkInstance` from the **system** loader (no `VK_ICD_FILENAMES` override unless the owner explicitly selects PanVK), picks the first physical device, and reports the identity from Task 5. Each suite becomes a function returning the three-way code. Attribute PanProbe in `NOTICE.md` with its MIT text.

- [ ] **Step 4: Make the ports runnable on the build host against lavapipe**

`run-host.sh` compiles the same C sources for Linux (`cc -lvulkan`) and runs each suite with `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`, which exists on this host. This validates the ports' plumbing without a phone; lavapipe results are **not** Mali evidence and the script says so in its output.

- [ ] **Step 5: Verify**

Run: `bash apps/dxvm-testpack/tests/vktests/run-host.sh && ./gradlew :app:testDebugUnitTest --tests '*VkResultParseTest*'`
Expected: each suite prints a PASS/FAIL/SKIP line under lavapipe (a `FAIL` there is acceptable — it means the driver lacks the feature); the Kotlin parse test passes.

- [ ] **Step 6: Commit**

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): port PanProbe Vulkan suites against the system ICD"
```

---

### Task 7: D3D suite — run dxvm-harness.exe in the container

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/D3DSuite.kt`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/D3DSuiteTest.kt`
- Add: `apps/dxvm-testpack/app/src/main/assets/harness/dxvm-harness.exe` (built by the DXVM repo)

**Interfaces:**
- Produces: `D3DSuite.run(ctx, api: String): Result<String>` returning the harness report text.
- Produces: `D3DSuite.parse(report: String): List<ResultLine>` where `ResultLine(kind, name, detail)`.

- [ ] **Step 1: Write the failing parse test**

```kotlin
class D3DSuiteTest {
    @Test fun parsesResultLinesAndSummary() {
        val text = """
            == Direct3D 11 suite (system driver) ==
            device created, feature level 0xb000
            PASS msaa2.sampleindex  red is 9, expected 9
            FAIL bcn.bc7            block fixture not ported yet
            SKIP compute            not ported yet
            SUMMARY 1 passed, 1 failed
        """.trimIndent()
        val lines = D3DSuite.parse(text)
        assertEquals(3, lines.size)
        assertEquals("msaa2.sampleindex", lines[0].name)
        assertEquals("PASS", lines[0].kind)
        assertEquals("FAIL", lines[1].kind)
        assertEquals("SKIP", lines[2].kind)
    }
    @Test fun emptyReportIsAFailureNotACleanRun() {
        assertTrue(D3DSuite.parse("").isEmpty())
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*D3DSuiteTest*'`
Expected: FAIL — unresolved `D3DSuite`.

- [ ] **Step 3: Implement `run` and `parse`**

`run` copies `dxvm-harness.exe` plus the installed DXVM DLLs into the prefix, launches it through the container with `--out`, waits with a bounded timeout, and reads the report. A missing report becomes a `CRASH` line naming the exit code — never an empty success. `parse` splits on the documented line kinds, tolerating the name field's internal spaces by finding the double-space separator.

- [ ] **Step 4: Verify and commit**

Run: `./gradlew :app:testDebugUnitTest --tests '*D3DSuiteTest*'`
Expected: PASS.

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): run the DXVM harness inside the container"
```

---

### Task 8: Report assembly and sharing

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/TestReport.kt`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/TestReportTest.kt`

**Interfaces:**
- Consumes: `DriverIdentity`, `D3DSuite.ResultLine`, `VkTests` lines.
- Produces: `TestReport.build(header: Header, d3d: List<ResultLine>, vk: List<ResultLine>): String` — the DXML-shaped document from the spec.

- [ ] **Step 1: Write the failing test**

```kotlin
class TestReportTest {
    @Test fun headerNamesTheDriverAndTotals() {
        val out = TestReport.build(
            Header(version = "0.1.0", code = 1, driver = "Driver: system, Mali-G615 MC2, driver r44p1", dxvm = "0.1.0-alpha.1-arm64ec"),
            listOf(D3DSuite.ResultLine("PASS", "device.create", "feature level 0xb000")),
            listOf(D3DSuite.ResultLine("FAIL", "vk.fill-mode", "polygonMode LINE rejected")))
        assertTrue(out.startsWith("DXVM TestPack report, "))
        assertTrue(out.contains("Driver: system, Mali-G615 MC2"))
        assertTrue(out.contains("Total: 1 passed, 1 failed or crashed"))
        assertTrue(out.contains("== Direct3D 11 suite"))
        assertTrue(out.contains("== Vulkan capabilities"))
    }
    @Test fun allSkipRunIsNotACleanRun() {
        val out = TestReport.build(Header("0.1.0", 1, "Driver: system", "x"),
            listOf(D3DSuite.ResultLine("SKIP", "compute", "not ported yet")), emptyList())
        assertTrue(out.contains("Total: 0 passed, 0 failed or crashed"))
        assertTrue(out.contains("NOTE: nothing ran"))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*TestReportTest*'`
Expected: FAIL — unresolved `TestReport`.

- [ ] **Step 3: Implement `build` plus the share action**

`build` emits the header, device block, one `== ... ==` section per half, `SUMMARY` per section, and the `== Totals (pass/total) ==` table (label 75 wide, three 11-wide columns, measured from the real DXML report). When nothing passed and nothing failed it appends `NOTE: nothing ran` so a dead container cannot read as a clean run. Sharing reuses PanPlay's existing log-share path.

- [ ] **Step 4: Verify and commit**

Run: `./gradlew :app:testDebugUnitTest --tests '*TestReportTest*'`
Expected: PASS.

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): assemble and share the DXML-shaped report"
```

---

### Task 9: Wire the test screen

**Files:**
- Create: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/TestScreen.kt`
- Modify: app entry point to open it
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/TestFlowTest.kt`

**Interfaces:**
- Consumes: Tasks 4–8.
- Produces: `suspend fun runFullPass(ctx, progress: (String) -> Unit): String` — installs/validates DXVM, probes the driver, runs both halves, builds the report.

- [ ] **Step 1: Write the failing ordering test**

```kotlin
class TestFlowTest {
    @Test fun stopsWhenDxvmIsNotInstalled() {
        val steps = mutableListOf<String>()
        val r = runFullPass(stubCtx(dxvmInstalled = false), steps::add)
        assertTrue(steps.any { it.contains("DXVM component") })
        assertTrue(r.contains("SKIP") || r.contains("FAIL"))
        assertFalse(r.contains("PASS device.create"))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*TestFlowTest*'`
Expected: FAIL — unresolved `runFullPass`.

- [ ] **Step 3: Implement the flow**

Order: prove DXVM is installed (Task 4 marker) → classify the driver (Task 5) → run the D3D half (Task 7) → run the Vulkan half (Task 6) → assemble (Task 8). Each step reports progress to the screen. A missing DXVM component short-circuits to an explicit failure, because testing upstream DXVK while claiming DXVM is the anti-goal.

- [ ] **Step 4: Verify and commit**

Run: `./gradlew :app:testDebugUnitTest`
Expected: all unit tests PASS.

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): one-button test pass"
```

---

### Task 10a: Optional comparison mode (system+DXVM vs PanVK+DXVK)

**Files:**
- Modify: `apps/dxvm-testpack/app/src/main/java/dev/zenithblue/panvklauncher/TestScreen.kt`
- Create: `apps/dxvm-testpack/app/src/test/java/dev/zenithblue/panvklauncher/ComparisonModeTest.kt`

**Interfaces:**
- Produces: `suspend fun runComparison(ctx, progress): String` — runs the full pass under the system driver with DXVM, then optionally under PanVK with PanPlay's stock DXVK component, and concatenates both reports with a separating banner.

- [ ] **Step 1: Write the failing test**

```kotlin
class ComparisonModeTest {
    @Test fun comparisonReportNamesBothDriverRuns() {
        val out = mergeComparison(
            "Driver: system, Mali-G615 MC2\nPASS msaa2.sampleindex  red is 9, expected 9",
            "Driver: PanVK beta.9\nFAIL msaa2.sampleindex  red is 26, expected 9")
        assertTrue(out.contains("== Run A: system driver + DXVM =="))
        assertTrue(out.contains("== Run B: PanVK + stock DXVK =="))
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./gradlew :app:testDebugUnitTest --tests '*ComparisonModeTest*'`
Expected: FAIL — unresolved `mergeComparison`.

- [ ] **Step 3: Implement `mergeComparison` and the optional second run**

The second run only happens when the owner explicitly selects PanVK; it is never the default and the banner must name both drivers so the two halves can never be confused.

- [ ] **Step 4: Verify and commit**

Run: `./gradlew :app:testDebugUnitTest --tests '*ComparisonModeTest*'`

```bash
git add apps/dxvm-testpack
git commit -m "feat(testpack): optional system vs PanVK comparison run"
```

---

### Task 10: CI artifact and owner run

**Files:**
- Modify: `.github/workflows/testpack.yml` (bundle the DXVM component into the APK assets via the existing component mechanism)

**Interfaces:**
- Produces: `dxvm-testpack-debug.apk` with `assets/components/dxvm-<version>-arm64ec.wcp` inside, installable on the owner's phone.

- [ ] **Step 1: Build the component in CI and place it as a bundled component**

Add a CI step that runs `tools/build-dxvm-component.sh` and copies the result to `app/src/main/assets/components/`, adding it to the component list so it unpacks on first start — mirroring how PanPlay bundles `dxvk-3.1.1-4-arm64ec.wcp`.

- [ ] **Step 2: Verify**

Run: the workflow; download the APK artifact; confirm it contains the `.wcp` and that the component verifier passed inside CI.

- [ ] **Step 3: Owner run**

Install on the phone, keep the **system driver** selected, press Start, share the report. The report must show `Driver: system`, a D3D section, a Vulkan section, and at least one result that differs from a PanVK run. That report is the input for the next DXVM fix round.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/testpack.yml apps/dxvm-testpack
git commit -m "build(testpack): bundle the DXVM component and publish the APK"
```