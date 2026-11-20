# DXVM Clean Architecture Design Document

**Date**: 2026-11-20
**Project**: DXVM (DirectX to Vulkan for Mali)
**Base**: DXVK 3.1.1 fork (current repository state — see §4.7 diff baseline)
**Target GPU**: Mali-G615 MC2 (ARM Valhall) — reference device
**Target Platform**: Windows DLLs (x64/x86) for Wine/Proton on aarch64 Mali hosts
**Approach**: A — Centralized Mali module inside DXVK (chosen by project owner)

---

## 1. Intent and Why

The current DXVM fork accumulated 10 critical fixes scattered across 13 upstream files after
repeated audits. The root cause is architectural: Mali-specific behavior was patched into
upstream DXVK paths ad hoc, and BCn texture translation was placed at *upload time* even though
Vulkan images are *created* before upload — on a Mali device that cannot create a BCn image, the
translation code is never reached. The owner asked for a from-scratch redesign so the final
product is clean and does not keep generating new problems.

This design replaces the scattered patches with one cohesive Mali module, moves the BCn format
decision to texture creation time, adds real fallbacks for GPU features Mali lacks, and adds a
CPU transcoder so BCn textures are never silently corrupted.

---

## 2. Goals / Non-Goals / Anti-Goals

### Goals
1. **One home for Mali logic**: extensions, features, hints, config, format policy, state emulation,
   and BCn transcoding live in a single module `src/dxvk/dxvm_mali/`. (Detection stays on
   `DxvkAdapter` — see §4.5; the module is the single home for *behavior*, detection remains the
   adapter's existing capability.)
2. **Minimal upstream footprint**: no more than ~5 hook sites in upstream DXVK files, each a
   one-to-three-line call into the module, clearly marked. (BCn front-end hooks bring this to 6 —
   see §4.4; accepted because format remap must happen before `DxvkImageCreateInfo` is finalized.)
3. **Correct BCn handling**: translate BCn → ASTC at texture *creation* (format remapping before
   `vkCreateImage`), with source-data transcoding at upload. GPU compute dispatch is the primary
   path; a CPU transcoder is the fallback so a texture is never uploaded as wrong-format bytes.
4. **Auditable**: a single config struct, a single Mali feature table, a single format-policy table.
5. **Preserve non-Mali behavior**: all hooks are no-ops when `isMali()` is false; upstream paths
   stay byte-for-byte equivalent for other vendors.
6. **Feature emulation (owner decision, Q1 = Option B)**: Mali lacks `dualSrcBlend`,
   `fillModeNonSolid`, `multiViewport`, `logicOp`, `depthBounds`. The module provides real
   emulation/clamping: wireframe → solid, dual-source blend factors → ordinary blend factors,
   extra viewports → clamp to 1, logic op → disabled, depth bounds → disabled. Games see working
   (if visually reduced) output rather than rejected Vulkan state.

### Non-Goals
- Reimplementing DXVK. Upstream remains the base; this is a clean integration layer, not a rewrite
  of the translation layer.
- Android/Winlator builds (removed earlier by owner decision). Windows x64/x86 for Wine/Proton only.
- Non-Mali GPU optimization work.
- Perfect visual fidelity for emulated features (wireframe-as-solid etc.) — correctness over fidelity.

### Anti-Goals
- No scattered `if (isMali)` checks across unrelated files.
- No feature declared in a struct without being chained into `VkDeviceCreateInfo` (the Sync2 bug).
- No required feature that the target GPU does not support (the dualSrcBlend bug).
- No placeholder transcoding presented as complete functionality, and no wrong-format byte upload.

---

## 3. Constraints

| Constraint | Detail |
|------------|--------|
| Base | DXVK 3.1.1 (current fork state) |
| Language / build | C++17, Meson, MinGW-w64 cross-compile |
| Target | Mali-G615 MC2, Vulkan 1.3.247, driver v1.r44p1+ |
| Mali limitations (from design doc) | no dualSrcBlend, no fillModeNonSolid, no logicOp, no depthBounds, 1 viewport, no native BCn |
| Unified memory | system RAM = VRAM; budget configurable (config-only today — see §7) |
| Exports | D3D8/9/10/11 + DXGI entry points unchanged |

---

## 4. Architecture

### 4.1 New module layout

```
src/dxvk/dxvm_mali/
├── dxvm_mali_context.h/.cpp      ← per-device entry point; owns all sub-objects
├── dxvm_mali_config.h/.cpp       ← one DxvmMaliConfig struct, parsed once from Config
├── dxvm_mali_features.h/.cpp     ← one MALI feature table (extension + feature + required flag)
├── dxvm_mali_hints.h/.cpp        ← perf hints (dynamic rendering, mipgen, descriptor offsets)
├── dxvm_mali_format_policy.h/.cpp← BCn→ASTC (incl. sRGB variants) mapping + creation-time remap
├── dxvm_mali_state_emulation.h/.cpp ← wireframe/blend/viewport/logic-op/depth-bounds fallbacks
├── dxvm_mali_bcn_transcoder.h/.cpp ← GPU dispatch + CPU fallback transcoder
├── bcndecode/                    ← small CPU BC1–BC7 decoder (no external dependency)
└── dxvm_mali_extension_provider.h/.cpp ← moved from dxvk_mali_extensions.*
```

BCn shaders (`src/dxvk/shaders/bcn/*.comp`) and `dxvk_meta_bcn.*` move under the module's
ownership; Meson references them from there. `dxvk_objects.h` keeps a single accessor to the
transcoder object supplied by the module.

`DxvmMaliContext` exposes:
- `const DxvmMaliConfig& config() const`
- `void registerExtensions(DxvkInstance&)` (provider registration)
- `void chainMaliFeatures(DxvkDeviceExtensionInfo&, DxvkDeviceFeatures&)` (adds Mali features to
  upstream chain — see §4.3)
- `void applyHints(DxvkDevicePerfHints&)`
- `VkFormat remapFormat(VkFormat)` (BCn→ASTC, including sRGB variants)
- `bool shouldEmulate(...)` / state fix-up helpers
- `DxvmBcnTranscoder& bcnTranscoder()`

### 4.2 Hook sites in upstream files

| # | File | Hook |
|---|------|------|
| 1 | `dxvk_instance.cpp` | register `DxvmMaliExtensionProvider::s_instance` (keep existing) |
| 2 | `dxvk_device_info.cpp/.h` | call `chainMaliFeatures(...)` inside `chainFeatures()`; keep upstream feature table intact |
| 3 | `dxvk_device.cpp` | call `applyHints()`, config-based shader options, emulation setup |
| 4 | `dxvk_context.cpp` | call `remapFormat()` when finalizing `DxvkImageCreateInfo`; call transcoder at upload |
| 5 | `d3d9/d3d9_adapter.cpp`, `dxgi/dxgi_adapter.cpp` | vendor reporting helper in module |
| 6 | `d3d9/d3d9_common_texture.cpp`, `d3d11/d3d11_texture.cpp` | call `remapFormat()` before `createImage`, and remap image-view format likewise |

Hook #6 is the reviewer-driven addition: the front ends build `DxvkImageCreateInfo` themselves,
so remapping must happen there too. Image views created later must use the remapped format (stored
on the image), so no separate view hook is needed beyond using `image->info().format`.

### 4.3 Feature table: upstream table + Mali override table (reviewer Q4)

There are two tables by design, not one, to satisfy both "single source of truth for Mali" and
"non-Mali byte-for-byte equivalent":
- **Upstream feature table** in `dxvk_device_info.cpp` stays exactly as DXVK 3.1.1 for all vendors.
  Its flags (`dualSrcBlend`, `fillModeNonSolid`, `multiViewport`, `textureCompressionBC`) return to
  their upstream values.
- **Mali override table** in `dxvm_mali_features.cpp` lists only the deltas for Mali: which features
  are disabled/optional, which ARM extensions are enabled, and required flags. `chainMaliFeatures()`
  applies these overrides *only when* `adapter->isMali()`. On non-Mali the override list is empty
  and upstream behavior is unchanged.

This resolves the contradiction: "single source of truth" applies to Mali-specific deltas, not to
the entire device feature set.

### 4.4 BCn strategy — creation-time remap + dual transcoder (reviewer Q2, Q3)

Flow:
1. Client API (D3D9 `D3D9CommonTexture`, D3D11 `D3D11CommonTexture`) resolves a format to
   `VkFormat` (e.g. `VK_FORMAT_BC3_UNORM_BLOCK`).
2. Before building/finalizing `DxvkImageCreateInfo`, call `DxvmMaliContext::remapFormat(vkFormat)`.
   On Mali: BC1/2/3/4/5 → ASTC 4x4 (sRGB variants → `ASTC_*_SRGB_BLOCK`), BC6H → ASTC 6x6
   SFLOAT, BC7 → ASTC 6x6 LDR/sRGB. On non-Mali: identity. Reviewer's sRGB gap (**Q7b**) is closed
   here — sRGB variants are explicitly mapped.
3. Image is created with the ASTC format; `vkCreateImage` always succeeds. The original BCn format
   is remembered per-texture for the transcoder.
4. At upload (`uploadImageHw`), transcode:
   - **Primary**: GPU compute dispatch using the existing 5 SPIR-V shaders + `dxvk_meta_bcn` pipeline.
   - **Fallback (owner decision, Q2 = Option A)**: CPU BCn decoder (`bcndecode/`) produces RGBA8,
     then uploads RGBA8 into the image. For ASTC-creation this means the image format is ASTC but
     the data is RGBA8 — invalid. Therefore the fallback re-decides the image format at creation:
     when GPU dispatch is unavailable, `remapFormat()` returns `VK_FORMAT_R8G8B8A8_UNORM` (or the
     sRGB pair) instead of ASTC, and the CPU decoder uploads matching RGBA8 bytes. No wrong-format
     byte upload ever occurs.
5. Transcoded textures are cached by (BCn data hash + format) to avoid repeated work.

The module decides primary vs fallback once per device (based on whether the compute pipeline can
be created) and exposes a stable `shouldUseCpuBcn()` that both creation and upload consult.

### 4.5 Detection stays on DxvkAdapter (reviewer Q5)

`DxvkAdapter::isMali()` and `getMaliArchitecture()` remain where they are — moving them would force
extra upstream edits and break the hook budget. The module is the single home for Mali *behavior*;
detection is the adapter's existing capability and the module consumes it. No contradiction.

### 4.6 State emulation (owner Q1 = Option B)

`dxvm_mali_state_emulation.cpp` provides:
- Wireframe (`D3D11_FILL_WIREFRAME` / `VK_POLYGON_MODE_LINE`) → solid.
- Dual-source blend factors → nearest ordinary blend factor; also clamp RT count to 1 for
  dual-source attachments.
- Viewport/scissor count → 1 (extra viewports ignored).
- Logic op → disabled (already reported as unsupported by D3D11 feature reporting; keep consistent).
- Depth bounds → disabled.

Applied from the device/context hooks (#3). Non-Mali: function returns immediately.

### 4.7 Diff baseline (reviewer Q6)

"Upstream" for verification means the **current fork state at the start of this redesign**, not
vanilla DXVK 3.1.1. The acceptance diff must show: only the 6 hook files changed + one new
`src/dxvk/dxvm_mali/` module + Meson edits + moved BCn shaders/meta objects. The prior 10 fixes are
the baseline, not something to recreate. `docs/audit-findings.md` is retained as history.

### 4.8 Config (reviewer Q7a)

Existing option keys `dxvk.enable*` and defaults are preserved when parsing into `DxvmMaliConfig`
so preset files and docs stay valid. Memory-budget enforcement is **out of scope** for this
redesign (config-only today); noted as a known limitation in §7.

### 4.9 HUD Revamp (owner decision)

The HUD is redesigned. The owner chose **Concept C1 — two-column compact panel**, with the status
line text `BCn→ASTC:GPU · DynR:ON · Sync2:ON`.

Layout (concept C1 final):
```
┌────────────────────────────────┐
│ DXVM · G615            Valhall │
│ FPS  60.2      FT   16.6ms     │
│ GPU  78%       DC   1,204      │
│ MEM  2.3/3.0G  CMP  idle       │
│ BCn→ASTC:GPU · DynR:ON · Sync2:ON│
└────────────────────────────────┘
```

- Elements: devinfo (short GPU/arch), FPS, frametime, GPU load, draw calls, memory, compiler,
  plus a status line reflecting DXVM optimizations (BCn mode, dynamic rendering, Sync2).
- Style: dark translucent panel, two columns, compact. Colors: FPS green, GPU amber, status grey.
- Hooks: implemented inside `src/dxvk/hud/` (existing HUD items extended/added); counts as part
  of the module work, not an extra upstream hook site.
- `BCn→ASTC:GPU` reflects GPU dispatch; when CPU fallback is active it reads `BCn→ASTC:CPU`.

---

## 5. Migration Plan (from current state)

1. Create `dxvm_mali/` module; move provider, config, hints, feature overrides, format policy,
   state emulation, and BCn transcoder (shaders + meta objects + CPU decoder) into it.
2. Reduce `dxvk_device_info.*` to the `chainMaliFeatures()` hook; restore upstream flags in the
   upstream table, put deltas in the Mali table.
3. Reduce `dxvk_device.cpp` to `applyHints()` + shader config + emulation setup; delete inline
   Mali blocks.
4. Move BCn decision to creation-time in D3D9/D3D11 texture constructors; wire image-view format
   from the remapped image.
5. Finish GPU compute dispatch in the module; implement CPU fallback decoder; add texture cache.
6. Reduce vendor hooks to one helper.
7. Revamp HUD to concept C1 (two-column compact panel + status line).
8. Verify against the §4.7 baseline.

---

## 6. Verification

- Windows x64 + x86 builds succeed.
- Exports: `D3D11CreateDevice`, `CreateDXGIFactory1/2`, `Direct3DCreate9`, `Direct3DCreate8`.
- Feature tables audited: upstream table matches DXVK 3.1.1; Mali override table chained only on
  Mali; no unsupported required features.
- Diff against baseline shows only 6 hook files + new module + Meson + moved BCn assets.
- Unit-style checks: `remapFormat()` identity for non-Mali; correct linear **and sRGB** ASTC mapping
  for Mali; CPU fallback yields correct RGBA8 bytes for each BCn format.
- Emulation checks: wireframe request produces solid Vulkan state on Mali; viewport count clamped.

---

## 7. Known Limitations / What's Left to the Builder

- Memory budget is config-only (not enforced in `dxvk_memory.cpp`); no hook budget for it.
- Emulated features are visually reduced by design (owner accepted correctness over fidelity).
- Builder decides `DxvmMaliContext` ownership (recommendation: `DxvkDevice`, matching perf-hints
  lifecycle), exact per-file edits, and migration order/regression checkpoints.
- Full GPU compute dispatch internals are left to the builder; CPU fallback must be complete
  before the first release so no wrong-format upload is possible.
