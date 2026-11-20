# DXVM Clean Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the scattered Mali patches in the DXVK fork with one cohesive `src/dxvk/dxvm_mali/` module, fix BCn at creation time with a CPU fallback, emulate Mali-missing features, and revamp the HUD to the approved C1 compact panel.

**Architecture:** A single Mali module owns all Mali-specific behavior (config, features, hints, format policy, state emulation, BCn transcoding). Upstream DXVK files are reduced to thin, clearly-marked hook calls (~6 sites). A two-table feature model keeps the upstream feature table byte-for-byte intact for non-Mali vendors while a Mali-only override table gates deltas. BCn formats are remapped to ASTC before `vkCreateImage`; GPU compute dispatch is primary and a CPU decoder is the fallback, so no texture is ever uploaded as wrong-format bytes.

**Tech Stack:** C++17, Meson, MinGW-w64 cross-compile (x86_64 + i686), Vulkan 1.3, SPIR-V via glslang, DXVK HUD rendering.

**Spec:** `docs/superpowers/specs/2026-11-20-dxvm-clean-architecture-design.md`

---

## Global Constraints

- Base: current DXVM fork at commit `2c9ac79` (the 10 prior fixes are the baseline, not to be recreated).
- Verification "upstream" = current fork baseline, **not** vanilla DXVK 3.1.1.
- Build targets: Windows x64 and x86 DLLs. No Android/Winlator target.
- Exports must remain: `D3D11CreateDevice`, `D3D11CreateDeviceAndSwapChain`, `CreateDXGIFactory`, `CreateDXGIFactory1`, `CreateDXGIFactory2`, `Direct3DCreate9`, `Direct3DCreate9Ex`, `Direct3DCreate8`, `D3D10CreateDevice`.
- Non-Mali behavior must stay byte-for-byte equivalent: all hooks no-op when `isMali()` is false.
- Config keys `dxvk.enable*` and defaults are preserved (preset compatibility).
- No required feature that Mali-G615 lacks (`dualSrcBlend`, `fillModeNonSolid`, `multiViewport`, `logicOp`, `depthBounds`, `textureCompressionBC`).
- C++17, Meson `cpp_std=c++17`; all new files added to `src/dxvk/meson.build`.
- Commit after every task with a `feat:` or `refactor:` message.

---

## Review Focus

| Failure mode / input | Expected behavior | Owning task |
|---|---|---|
| Non-Mali GPU (NVIDIA/AMD/Intel) with HUD enabled | Module hooks are no-ops; feature table unchanged; HUD renders existing items if configured | Task 2, Task 8 |
| Mali device that lacks one ARM extension (e.g. no `VK_EXT_shader_tile_image`) | Feature gated off, no crash, no required-feature rejection | Task 2 |
| BCn texture (linear **and sRGB**) on Mali with GPU dispatch available | Image created as ASTC (sRGB→ASTC sRGB); GPU transcode produces correct pixels | Task 4, Task 5 |
| BCn texture when GPU compute pipeline cannot be created | `remapFormat()` returns RGBA8 (matching sRGB pair); CPU decoder fills correct RGBA8 bytes; never a wrong-format upload | Task 5 |
| Game requests wireframe / dual-source blend / >1 viewport / logic op on Mali | Emulation produces valid Vulkan state (solid, ordinary blend, 1 viewport, logic op off); no `vkCreateGraphicsPipelines` rejection | Task 6 |

---

## File Structure

**Module API convention (resolves spec §4.1):** all module behavior is exposed as free functions
with the `dxvmMali*` prefix (e.g. `dxvmMaliChainFeatures`, `dxvmMaliApplyHints`,
`dxvmMaliRemapFormat`, `dxvmMaliEmulateGraphicsState`, `dxvmIsClassicVendor`). `DxvmMaliContext`
is the per-device holder of `isMali` + `DxvmMaliConfig` + owned sub-objects, passed to those
functions where needed. This keeps the functions directly unit-testable without a device.

```
src/dxvk/dxvm_mali/
├── dxvm_mali_context.h/.cpp          per-device entry point, owns sub-objects
├── dxvm_mali_config.h/.cpp           DxvmMaliConfig + preserved dxvk.* parsing
├── dxvm_mali_features.h/.cpp         Mali-only override feature table
├── dxvm_mali_hints.h/.cpp            perf hints
├── dxvm_mali_format_policy.h/.cpp    BCn→ASTC/sRGB + remapFormat()
├── dxvm_mali_state_emulation.h/.cpp  wireframe/blend/viewport/logic/depth emulation
├── dxvm_mali_bcn_transcoder.h/.cpp   GPU dispatch + CPU fallback + cache
├── bcndecode/bcn_decode.h/.cpp       CPU BC1–BC7 decoder → RGBA8
└── dxvm_mali_extension_provider.h/.cpp (moved from dxvk_mali_extensions.*)
src/dxvk/shaders/bcn/*.comp           moved under module ownership (Meson updated)
src/dxvk/dxvk_meta_bcn.*              moved under module ownership
src/dxvk/hud/*                        HUD C1 panel
```

---

## Task 1: Module skeleton and config

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_config.h`, `src/dxvk/dxvm_mali/dxvm_mali_config.cpp`
- Create: `src/dxvk/dxvm_mali/dxvm_mali_context.h`, `src/dxvk/dxvm_mali/dxvm_mali_context.cpp`
- Modify: `src/dxvk/meson.build` (add `dxvm_mali` sources)
- Create: `tests/dxvm/run.sh` (test harness: compiles each `tests/dxvm/test_*.cpp` against the module sources with `g++ -std=c++17 -I include -I include/vulkan/include -I src`, runs the binary; `all` loops over every test)

**Interfaces:**
- Consumes: `Config` (`../util/config/config.h`), `DxvkAdapter::isMali()` via a passed-in flag (no include of adapter in config).
- Produces: `DxvmMaliConfig` with fields for all 12+ existing `dxvk.*` Mali options and defaults; `DxvmMaliContext(bool isMali, const Config&)` with `config()` and `isMali()`.

- [ ] **Step 1: Write test harness and the config test**

Create `tests/dxvm/run.sh` as described in Files. Create `tests/dxvm/test_config.cpp` asserting:
  - `DxvmMaliConfig cfg(Config());` yields `enableBcnTranscoding == true`, `enableDynamicRendering == true`, `maxMemoryBudgetMB == 0` (config default), `enableSynchronization2 == false` (upstream default).
  - A `Config` with `dxvk.enableARMBuiltins = false` yields `cfg.enableARMBuiltins == false`.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/dxvm/run.sh config`
Expected: FAIL (compile error: header missing).

- [ ] **Step 3: Implement config and context**

`DxvmMaliConfig` constructor reads each existing key with its current default (copy verbatim from `dxvk_options.cpp`). `DxvmMaliContext` stores `bool m_isMali` and `DxvmMaliConfig m_config`, exposes `bool isMali() const` and `const DxvmMaliConfig& config() const`. No other dependencies.

- [ ] **Step 4: Add sources to Meson and run test**

Modify `src/dxvk/meson.build` to add the four new `.cpp` files to `dxvk_src`. Run `bash tests/dxvm/run.sh config`.
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/dxvk/dxvm_mali tests/dxvm src/dxvk/meson.build
git commit -m "feat: add DXVM Mali module skeleton and config"
```

---

## Task 2: Feature override table and extension provider

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_features.h`, `src/dxvk/dxvm_mali/dxvm_mali_features.cpp`
- Create: `src/dxvk/dxvm_mali/dxvm_mali_extension_provider.h`, `src/dxvk/dxvm_mali/dxvm_mali_extension_provider.cpp` (move contents of `dxvk_mali_extensions.*`)
- Modify: `src/dxvk/dxvk_device_info.cpp` (restore upstream table flags, add one `chainMaliFeatures` call)
- Modify: `src/dxvk/dxvk_instance.cpp` (provider registration uses moved class)
- Delete: `src/dxvk/dxvk_mali_extensions.h`, `.cpp`
- Modify: `src/dxvk/meson.build`

**Interfaces:**
- Consumes: `DxvmMaliContext::isMali()`; `DxvkDeviceExtensionInfo&`, `DxvkDeviceFeatures&` from `dxvk_device_info.h`.
- Produces: `void dxvmMaliChainFeatures(bool isMali, DxvkDeviceExtensionInfo& extensions, DxvkDeviceFeatures& features)`; `class DxvmMaliExtensionProvider : public DxvkExtensionProvider` with same public API as old provider.

- [ ] **Step 1: Write the feature-table test**

Create `tests/dxvm/test_features.cpp`:
  - With `isMali = false`, call `dxvmMaliChainFeatures` and assert no field in `DxvkDeviceFeatures` changed and no extension specVersion changed.
  - With `isMali = true`, assert `features.armShaderCoreBuiltins.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM` and that the extension entry for `VK_ARM_SHADER_CORE_BUILTINS_EXTENSION_NAME` has non-zero specVersion.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/dxvm/run.sh features`
Expected: FAIL (function undefined).

- [ ] **Step 3: Restore upstream feature flags and implement override table**

In `dxvk_device_info.cpp`, restore `dualSrcBlend`, `fillModeNonSolid`, `multiViewport`, `textureCompressionBC`, and the 4 ARM entries to their upstream values/removal (delete local ARM structs; delete `khrSynchronization2`/`khrTimelineSemaphore` local entries if upstream lacks them — they live in the override table now). Implement `dxvmMaliChainFeatures` applying only Mali deltas. Move the provider files, keeping class name changed only where registration refers to it.

- [ ] **Step 4: Add one hook in `chainFeatures()`**

Inside `DxvkDeviceCapabilities::chainFeatures`, after `CORE_VERSIONS`/`EXTENSIONS_WITH_FEATURES`, add:
```cpp
dxvmMaliChainFeatures(m_properties.vk12.driverID == VK_DRIVER_ID_ARM_PROPRIETARY
                   || m_properties.vk12.driverID == VK_DRIVER_ID_MESA_PANVK,
                   extensions, features);
```
Run `bash tests/dxvm/run.sh features`.
Expected: PASS.

- [ ] **Step 5: Verify build and commit**

Run `meson compile -C build-win64`.
Expected: PASS with all 5 DLLs linked.
```bash
git add -A src/dxvk/dxvm_mali src/dxvk/dxvk_device_info.cpp src/dxvk/dxvk_instance.cpp src/dxvk/meson.build tests/dxvm
git commit -m "refactor: centralize Mali feature overrides and extensions"
```

---

## Task 3: Perf hints and shader options

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_hints.h`, `src/dxvk/dxvm_mali/dxvm_mali_hints.cpp`
- Modify: `src/dxvk/dxvk_device.cpp` (replace inline Mali blocks with module calls)

**Interfaces:**
- Consumes: `DxvmMaliContext`, `DxvkDevicePerfHints` (`dxvk_device.h`), `DxvkShaderOptions`.
- Produces: `void dxvmMaliApplyHints(const DxvmMaliConfig&, VkDriverId driverId, bool isValhall, DxvkDevicePerfHints& hints)`; `void dxvmMaliApplyShaderOptions(const DxvmMaliConfig&, bool isMali, bool hasFloat16, bool hasInt8, DxvkShaderOptions& options)`.

- [ ] **Step 1: Write hints test**

Create `tests/dxvm/test_hints.cpp` asserting:
  - With `isMali=false`, `dxvmMaliApplyHints` changes nothing.
  - With `isMali=true`, `enableDynamicRendering=true`, Valhall: `hints.preferDynamicRendering == true`, `hints.preferComputeMipGen == true`, `hints.preferDescriptorByteOffsets == true`.

- [ ] **Step 2: Run test to verify it fails** → `bash tests/dxvm/run.sh hints` → FAIL.

- [ ] **Step 3: Implement hints and shader options**

Move the hint logic from `dxvk_device.cpp:697-742` and shader flags from `758-769` into module functions with the signatures above.

- [ ] **Step 4: Replace inline blocks with two calls in `dxvk_device.cpp`** and run `bash tests/dxvm/run.sh hints`.
Expected: PASS. Then `meson compile -C build-win64` → PASS.

- [ ] **Step 5: Commit** with message `refactor: move Mali perf hints into module`.

---

## Task 4: Format policy — BCn remap at creation

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_format_policy.h`, `src/dxvk/dxvm_mali/dxvm_mali_format_policy.cpp`
- Modify: `src/dxvk/dxvk_format.cpp` (replace bodies of `getBcnTranscodeFormat`/`needsBcnTranscoding` with thin forwarding calls to the module; the wrappers are deleted in Task 9)
- Modify: `src/d3d9/d3d9_common_texture.cpp`, `src/d3d11/d3d11_texture.cpp` (call remap before `createImage`)

**Interfaces:**
- Consumes: `bool isMali`, `bool useCpuBcn` (from transcoder selection), `VkFormat`.
- Produces: `VkFormat dxvmMaliRemapFormat(bool isMali, bool useCpuBcn, VkFormat format)`.

- [ ] **Step 1: Write remap test**

Create `tests/dxvm/test_format.cpp` asserting:
  - `dxvmMaliRemapFormat(false, false, VK_FORMAT_BC3_UNORM_BLOCK) == VK_FORMAT_BC3_UNORM_BLOCK` (identity).
  - `dxvmMaliRemapFormat(true, false, VK_FORMAT_BC1_RGBA_UNORM_BLOCK) == VK_FORMAT_ASTC_4x4_UNORM_BLOCK`.
  - `dxvmMaliRemapFormat(true, false, VK_FORMAT_BC1_RGBA_SRGB_BLOCK) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK`.
  - `dxvmMaliRemapFormat(true, false, VK_FORMAT_BC7_SRGB_BLOCK) == VK_FORMAT_ASTC_6x6_SRGB_BLOCK`.
  - `dxvmMaliRemapFormat(true, true, VK_FORMAT_BC3_UNORM_BLOCK) == VK_FORMAT_R8G8B8A8_UNORM` (CPU fallback).
  - `dxvmMaliRemapFormat(true, true, VK_FORMAT_BC3_SRGB_BLOCK) == VK_FORMAT_R8G8B8A8_SRGB`.

- [ ] **Step 2: Run test to verify it fails** → FAIL.

- [ ] **Step 3: Implement remap table**

Table exactly as spec §4.4: BC1/2/3/4/5 → ASTC 4x4 (matching sRGB), BC6H → ASTC 6x6 SFLOAT, BC7 → ASTC 6x6 (matching sRGB). CPU fallback → RGBA8 (matching sRGB). Identity for non-Mali.

- [ ] **Step 4: Wire front-end hooks**

In `d3d9_common_texture.cpp:411` and `d3d11_texture.cpp:28`, before `createImage`, set:
```cpp
imageInfo.format = dxvmMaliRemapFormat(isMali, useCpuBcn, imageInfo.format);
```
Use `image->info().format` (already remapped) for view creation — no separate view hook. Run `bash tests/dxvm/run.sh format` → PASS.

- [ ] **Step 5: Verify build and commit** (`refactor: remap BCn formats at texture creation`).

---

## Task 5: BCn transcoder — GPU dispatch + CPU fallback

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_bcn_transcoder.h`, `.cpp`
- Create: `src/dxvk/dxvm_mali/bcndecode/bcn_decode.h`, `.cpp`
- Move: `src/dxvk/dxvk_meta_bcn.*` → module; `src/dxvk/shaders/bcn/*.comp` → module path
- Modify: `src/dxvk/dxvk_context.cpp` (upload calls transcoder with remapped image), `src/dxvk/dxvk_objects.h` (single accessor), `src/dxvk/meson.build`

**Interfaces:**
- Consumes: remapped image format, original BCn format (carried per texture), source buffer slice.
- Produces: `class DxvmBcnTranscoder { bool useCpuFallback() const; VkResult transcode(const DxvkBuffer& src, VkDeviceSize srcOffset, VkFormat bcnFormat, const DxvkBuffer& dst); }`; `rgba8 decodeBcnBlock(const uint8_t* block, VkFormat format)` in `bcn_decode.h`.

- [ ] **Step 1: Write CPU decoder test**

Create `tests/dxvm/test_bcn_decode.cpp`: feed a known BC1 block (master colors red/blue, known 2-bit indices) and assert decoded RGBA8 matches expected colors; repeat for one BC7 mode block from a fixture.

- [ ] **Step 2: Run test to verify it fails** → FAIL.

- [ ] **Step 3: Implement CPU decoder**

`bcn_decode.cpp`: implement BC1 (RGB565 endpoint lerp), BC2/3 (alpha block), BC4/5, BC6H, BC7 using a compact open decoder implementation with no external dependency. Output RGBA8.

- [ ] **Step 4: Implement transcoder selection and GPU dispatch**

`DxvmBcnTranscoder` constructor tries to create the 5 shader pipelines from the module-owned shaders; `useCpuFallback()` returns true if creation fails. `transcode()` dispatches GPU path when available, otherwise calls `decodeBcnBlock` into a staging RGBA8 buffer. Add a texture cache keyed by (data hash + format).

- [ ] **Step 5: Wire upload path and verify**

In `uploadImageHw`, call `transcoder.transcode(...)` then copy to the (already remapped) image. Remove the old placeholder `uploadImageBcnTranscode` recursion-guard complexity. Run tests → PASS; `meson compile -C build-win64` and `-C build-win32` → PASS.

- [ ] **Step 6: Commit** (`feat: complete BCn transcoding with CPU fallback`).

---

## Task 6: State emulation for Mali-missing features

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_state_emulation.h`, `.cpp`
- Modify: `src/dxvk/dxvk_device.cpp` (call setup), `src/dxvk/dxvk_context.cpp` or graphics state path (apply clamps)

**Interfaces:**
- Consumes: `bool isMali`.
- Produces: `void dxvmMaliEmulateGraphicsState(bool isMali, VkGraphicsPipelineCreateInfo& pipelineInfo, VkPipelineColorBlendAttachmentState* attachments, uint32_t& attachmentCount, uint32_t& viewportCount)`.

- [ ] **Step 1: Write emulation test**

Create `tests/dxvm/test_emulation.cpp` asserting on a synthetic `VkGraphicsPipelineCreateInfo` with `VK_POLYGON_MODE_LINE`: after call with `isMali=true`, polygon mode is `VK_POLYGON_MODE_FILL`; dual-source blend factor maps to `VK_BLEND_FACTOR_ONE`; `viewportCount` becomes 1; logicOp enable is false. With `isMali=false`, no change.

- [ ] **Step 2: Run test to verify it fails** → FAIL.

- [ ] **Step 3: Implement emulation function** with exact mappings from spec §4.6.

- [ ] **Step 4: Hook into graphics state creation** (one call in the pipeline/context path) and run test → PASS; build → PASS.

- [ ] **Step 5: Commit** (`feat: emulate Mali-missing graphics features`).

---

## Task 7: Vendor reporting helper

**Files:**
- Create: `src/dxvk/dxvm_mali/dxvm_mali_vendor.h`, `.cpp`
- Modify: `src/d3d9/d3d9_adapter.cpp`, `src/dxgi/dxgi_adapter.cpp` (use helper)

**Interfaces:**
- Consumes: raw Vulkan vendorID.
- Produces: `bool dxvmIsClassicVendor(uint32_t vendorId)` returning true for NVIDIA/AMD/Intel/**ARM**.

- [ ] **Step 1: Write vendor test** — `tests/dxvm/test_vendor.cpp` asserts `dxvmIsClassicVendor(0x13b5) == true` and non-classic for a random ID.

- [ ] **Step 2: Run test to verify it fails** → FAIL.

- [ ] **Step 3: Implement helper; replace the two scattered `isNonclassicalVendorId`-style checks with it** (D3D9 and DXGI).

- [ ] **Step 4: Run test → PASS; build → PASS.**

- [ ] **Step 5: Commit** (`refactor: centralize Mali vendor reporting`).

---

## Task 8: HUD C1 revamp

**Files:**
- Modify: `src/dxvk/hud/dxvk_hud.cpp`, `src/dxvk/hud/dxvk_hud_item.h`, `src/dxvk/hud/dxvk_hud_item.cpp`
- Optional create: `src/dxvk/hud/dxvk_hud_dxvm_status.h/.cpp` for the status line

**Interfaces:**
- Consumes: existing HUD items (`HudFpsItem`, `HudFrameTimeItem`, `HudGpuLoadItem`, `HudDrawCallStatsItem`, `HudMemoryStatsItem`, `HudCompilerActivityItem`), `DxvmMaliContext` for status text.
- Produces: one `DXVM_HUD` option that renders the two-column C1 panel + status line `BCn→ASTC:GPU` (or `:CPU`) `· DynR:ON · Sync2:ON`.

- [ ] **Step 1: Write HUD text test** — `tests/dxvm/test_hud_text.cpp` asserts the status string exactly equals `"BCn→ASTC:GPU · DynR:ON · Sync2:ON"` when GPU dispatch active and `"BCn→ASTC:CPU · DynR:ON · Sync2:ON"` for CPU fallback.

- [ ] **Step 2: Run test to verify it fails** → FAIL.

- [ ] **Step 3: Implement C1 panel**

Add a new HUD item or compose existing items into the two-column layout; render short devinfo (`DXVM · G615`, `Valhall`), FPS, FT, GPU, DC, MEM, CMP, then the status line. Use existing renderer primitives; no new font/shader required.

- [ ] **Step 4: Run test → PASS; build both targets → PASS.**

- [ ] **Step 5: Commit** (`feat: revamp DXVM HUD to C1 compact panel`).

---

## Task 9: Migration cleanup and final verification

**Files:**
- Modify: `src/dxvk/dxvk_format.cpp`, `src/dxvk/dxvk_options.cpp/.h`, `src/dxvk/dxvk_objects.h`, `src/dxvk/meson.build`
- Update: `docs/audit-findings.md` (mark resolved), README/config docs if present

**Interfaces:**
- Consumes: all module APIs from Tasks 1–8.
- Produces: final tree where upstream files contain only the 6 hook sites + new module.

- [ ] **Step 1: Remove legacy scattered code**

Delete from `dxvk_format.cpp` the old BCn helpers; from `dxvk_options.*` the Mali fields (moved to config); from `dxvk_objects.h` the old `metaBcnTranscode` accessor (replaced by module accessor). Keep config key parsing delegated to module.

- [ ] **Step 2: Audit diff against baseline**

Run: `git diff 2c9ac79 --stat`. Expected: only 6 hook files changed + new module + Meson + moved assets + HUD files.

- [ ] **Step 3: Full build and export verification**

Run: `meson compile -C build-win64 && meson compile -C build-win32`, then:
```bash
x86_64-w64-mingw32-objdump -p dist/win64/d3d11.dll | grep D3D11CreateDevice
i686-w64-mingw32-objdump -p dist/win32/dxgi.dll | grep CreateDXGIFactory1
```
Expected: all exports present.

- [ ] **Step 4: Run all module tests**

Run: `bash tests/dxvm/run.sh all`
Expected: all 8 test files PASS.

- [ ] **Step 5: Commit** (`refactor: complete DXVM clean architecture migration`).
