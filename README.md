# DXVM

**DirectX to Vulkan for Mali** — a fork of [DXVK 3.1.1](https://github.com/doitsujin/dxvk) optimized for ARM Mali GPUs, targeting **Mali-G615 MC2 (Valhall)**.

DXVM ships Windows DLLs (`d3d8`, `d3d9`, `d3d10core`, `d3d11`, `dxgi`) for use under **Wine / Proton**. On a Mali GPU it works around features the driver lacks, transcodes BCn textures, applies Valhall-specific performance hints, and exposes a compact status HUD. On every other GPU, behavior is byte-for-byte equivalent to upstream DXVK.

> **Status:** architecture cleanup complete. See [current limitations](#current-limitations) before use.
> Development: [`dxvm-clean-arch`](https://github.com/danzfadlullah/DXVM/pull/1) · [design spec](docs/superpowers/specs/2026-11-20-dxvm-clean-architecture-design.md) · [implementation plan](docs/superpowers/plans/2026-11-20-dxvm-clean-architecture.md)

---

## Why DXVM?

Mali GPUs do not natively support several features Windows games expect, and have no hardware BCn (BC1–BC7) texture support. Upstream DXVK therefore either rejects the device or renders incorrectly on these GPUs. DXVM fixes this in one centralized module rather than scattered patches:

| Problem | DXVM solution |
|---|---|
| Device rejected for missing `dualSrcBlend`, `fillModeNonSolid`, `multiViewport`, `textureCompressionBC` | Those flags are optional in the feature table; a Mali-only override table gates deltas and enables supported ARM/KHR extensions |
| Games ship BC1–BC7 textures, Mali cannot sample them | BCn is remapped at texture creation to ASTC (GPU path) or RGBA8 (CPU path); original format is tracked per texture so uploads are never wrong-format |
| Wireframe / dual-source blend / multiple viewports / logic ops unsupported | Graphics state is clamped to valid Mali state before pipeline creation |
| HUD gives no Mali-specific information | Opt-in `DXVK_HUD=dxvm` compact two-column panel with a live status line |

The target platform is Windows DLLs for Wine/Proton. Android/Termux/Winlator/Box64 is **not** a target of this fork.

---

## Installation

Install exactly like DXVK — DXVM uses the same DLL names and exports.

Copy the DLLs from `dist/win64` and `dist/win32` into your Wine prefix, then add native DLL overrides for `d3d8`, `d3d9`, `d3d10core`, `d3d11` and `dxgi` in `winecfg`:

```bash
export WINEPREFIX=/path/to/wineprefix
cp dist/win64/*.dll "$WINEPREFIX/drive_c/windows/system32"
cp dist/win32/*.dll "$WINEPREFIX/drive_c/windows/syswow64"
winecfg   # Libraries tab -> add native overrides
```

For a pure 32-bit prefix, copy the 32-bit DLLs into `system32` instead. Enable `DXVK_HUD=dxvm` to confirm DXVM is active.

DLL requirements per API are unchanged from DXVK:

- `d3d8`: `d3d8.dll` + `d3d9.dll`
- `d3d9`: `d3d9.dll`
- `d3d10`: `d3d10core.dll` + `d3d11.dll` + `dxgi.dll`
- `d3d11`: `d3d11.dll` + `dxgi.dll`

To remove DXVM, delete the DLLs and overrides and run `wineboot -u`.

**Online games:** replacing Direct3D libraries may be treated as cheating and can get an account banned. Use at your own risk.

---

## HUD

In addition to all upstream `DXVK_HUD` options, DXVM adds:

- **`dxvm`** — compact C1 panel: GPU name (`DXVM · G615`), architecture, FPS, frame time, GPU load, draw calls, memory, compiler activity, and a status line:

```
BCn→ASTC:CPU · DynR:ON · Sync2:ON
```

`BCn→ASTC` shows `GPU` or `CPU` depending on the transcode path; `DynR` and `Sync2` report whether dynamic rendering and synchronization2 are active. Enable with `DXVK_HUD=dxvm`, optionally combined with other items, e.g. `DXVK_HUD=dxvm,fps`.

---

## Configuration

DXVM keeps all existing `dxvk.*` options. Mali-specific options are parsed once by `DxvmMaliConfig`:

| Option | Default | Purpose |
|---|---|---|
| `dxvk.enableBcnTranscoding` | `true` | Remap/transcode BCn textures on Mali |
| `dxvk.enableDynamicRendering` | `true` | Prefer `VK_KHR_dynamic_rendering` on Mali |
| `dxvk.enableFP16Shaders` | `false` | Allow 16-bit arithmetic (auto-enabled on Mali when supported) |
| `dxvk.enableINT8Shaders` | `false` | Allow INT8 arithmetic (auto-enabled on Mali when supported) |
| `dxvk.enableDescriptorIndexing` | `false` | Descriptor indexing hint |
| `dxvk.enableBufferDeviceAddress` | `false` | Buffer device address hint |
| `dxvk.enableShaderTileImage` | `false` | `VK_EXT_shader_tile_image` |
| `dxvk.enableARMBuiltins` | `false` | `VK_ARM_shader_core_builtins` |
| `dxvk.enableSynchronization2` | `false` | `VK_KHR_synchronization2` |
| `dxvk.enableTimelineSemaphore` | `false` | `VK_KHR_timeline_semaphore` |
| `dxvk.preferCachedMemory` | `auto` | Prefer cached memory on unified-memory systems |
| `dxvk.maxMemoryBudget` | `0` | Memory budget in MB (reporting only; not enforced) |

All Mali hooks are gated on `isMali()`, so these options have no effect on other GPUs.

---

## Building

Requirements (same as DXVK): Meson ≥ 0.58, glslang, and a mingw-w64 cross compiler with POSIX threads.

```bash
git clone --recursive https://github.com/danzfadlullah/DXVM.git
cd DXVM

# 64-bit DLLs
meson setup --cross-file build-win64.txt --buildtype release build-win64
meson compile -C build-win64

# 32-bit DLLs
meson setup --cross-file build-win32.txt --buildtype release build-win32
meson compile -C build-win32
```

Finished DLLs are produced under `build-win*/src/<api>/`. Copy them to `dist/win64` / `dist/win32` for installation.

Host-side unit tests for the Mali module do not need Windows:

```bash
bash tests/dxvm/run.sh all
```

Ten tests cover config, context, feature overrides, format remap, BCn decoding and subresource layout, perf hints, state emulation, HUD text, and vendor reporting. Build verification also checks exported symbols (`D3D11CreateDevice`, `CreateDXGIFactory1`) for both architectures.

If mingw-w64 lacks POSIX threads you may see `std::cv_status` errors. On Debian/Ubuntu, select the `posix` alternative for the four `*-mingw32-gcc/g++` compilers with `update-alternatives --config`.

---

## Project layout

```
src/dxvk/dxvm_mali/     All Mali behavior (config, features, hints,
                        format policy, BCn transcoder + decoder,
                        state emulation, vendor helper, extension provider)
src/dxvk/hud/           C1 HUD panel and DXVM status line
tests/dxvm/             Host-side unit tests + Windows stubs
docs/superpowers/       Design spec and implementation plan
```

Upstream DXVK files contain only the hook sites that call into the module.

---

## Current limitations

These are deliberate and documented, not bugs:

- **BCn GPU dispatch is deferred.** The CPU decoder is the active path, so the HUD always reports `BCn→ASTC:CPU`. The ASTC remap path exists but is not driven by a compute pipeline yet.
- **BC6H/BC7 CPU decode is approximate**; BC4/BC5 SNORM data is treated as UNORM. No texture cache is implemented.
- **Memory budget** is configuration-only and not enforced.
- **Reduced fidelity is accepted** for emulated wireframe, dual-source blending, and multiple viewports — correctness over visual fidelity.
- **Android/Termux/Winlator/Box64** is no longer a target.

Non-Mali behavior remains byte-for-byte equivalent to upstream DXVK.

---

## Credits & license

Based on [DXVK](https://github.com/doitsujin/dxvk) by Philip Rebohle, Joshua Ashton, Robin Kertels, Jeffrey Ellison and contributors. See [LICENSE](LICENSE) for the upstream license (zlib).
