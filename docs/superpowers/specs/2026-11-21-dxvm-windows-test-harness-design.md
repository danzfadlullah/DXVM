# DXVM Windows Test Harness — Design

**Date:** 2026-11-21
**Status:** Draft for owner approval

## Intent

Build a Windows test harness that reproduces the DXML 1.48 test matrix for DXVM,
so the owner can run it under Wine/Proton with the DXVM DLLs and send back a
structured log (`PASS` / `FAIL` / `CRASH` lines plus device info) for the next
development round.

The owner explicitly chose **Approach A: full matrix reproduction** over a
DXVM-focused smoke harness. This document records what "full matrix" means in
this environment and what is deliberately out of scope.

## Why

- The DXML Android Test Pack is a mature, detailed Direct3D compatibility suite.
  Its `PASS/FAIL/CRASH` output and scene names are the de facto language the
  owner and I already use to discuss GPU behaviour.
- DXVM targets Windows DLLs for Wine/Proton. Running the same scenes against
  DXVM gives directly actionable failures tied to DXVM's module
  (`dxvm_mali/`).
- The owner wants to supply logs after each run; the harness must therefore
  emit one self-contained report file that can be pasted into a chat.

## Goals

1. A Windows console executable built from this repo, runnable under Wine with
   DXVM DLLs in the prefix.
2. Covers D3D11, D3D10 and D3D9 device creation and rendering tests.
3. Reproduces the DXML scene matrix (all 73 scene functions found in
   `libdxmlpack.so`) as C++ test cases compiled into the harness.
4. Supports three backends per API, mirroring DXML: raw driver (`DXVK_VULKAN_LIB`
   unset), Mali wrapper, and wrapper forced mode. On Windows these map to
   Vulkan ICD selection through `VK_ICD_FILENAMES` / driver override, not the
   Android `libvulkan_wrapper.so`.
5. Emits a report with: device summary (GPU, driver, Vulkan version, DXVM
   version), one line per scene in `PASS name` / `FAIL name: reason` /
   `CRASH name: signal` form, and a final `N of M passed` summary.
6. Optional env file for extra switches (mirroring DXML `wrapper_env.txt`):
   `--env-file` reads `KEY=VALUE` lines and sets them before device creation.
7. Optional `--scenes` filter to run a subset by name (mirrors DXML
   `dev_scenes.txt`), and `--api d3d11|d3d10|d3d9` selection.
8. A launcher script for Wine that sets DLL overrides and captures stdout plus
   the report file into one log.

## Non-goals

- Android `.so` builds, APK packaging, or the Mali Vulkan wrapper itself.
- Reproducing the DXVK 1.9.2 / 2.7.1 / 3.1.1 multi-engine matrix from inside
  one process. The harness always loads the DLLs in its own directory (DXVM);
  comparing against upstream DXVK is done by running the harness in a second
  prefix with upstream DLLs.
- Shader compiler internals, DWARF `DW_OP_piece` deep-dive beyond what the
  scene needs to render, and Winlator/Box64.

## Anti-goals

- A harness that reports `PASS` when `D3D11CreateDevice` failed and no pixels
  were produced. Every scene must render or be explicitly skipped with a
  visible reason.
- Output that requires post-processing to be readable. The report must be
  paste-ready.

## Constraints

- C++17 (same as DXVM), MinGW-w64 cross build from this repo, plus MSVC-compatible
  sources where practical.
- No new external dependency for image comparison — use in-process readback and
  simple numeric/pixel checks.
- Must work with DXVM's current export set; D3D8 is not included (DXML 1.48
  bundles no `d3d8` pack).
- Wine has no reliable `SIGSEGV`-to-call-stack mapping; `CRASH` lines should
  report the last scene name and a generic signal string, not invent a stack.

## Scene matrix (source of truth)

The 73 scene functions extracted from `libdxmlpack.so` in the latest DXML Test Pack
(authoritative list; the earlier build had 71, and "67" was a miscount):

`Alu`, `Append`, `AsyncCompile`, `Atomics`, `Ballot32`, `Ballot32Sync`,
`BlendFactor`, `BlendPrims`, `Blend`, `Border`, `Compute`, `Copy`, `Cube`,
`CullDistance`, `DepthClamp`, `DepthClip`, `Depth`, `Discard`,
`DiscardSemantics`, `DispatchIndirect`, `DrawIndirect`, `DualSource`,
`ErrClamp`, `ErrCull`, `ErrStrides`, `Formats`, `FrontFace`, `GatherOffset`,
`Geometry`, `GroupShared`, `Instancing`, `InternalPasses`, `LogicOp`,
`MapNoOverwrite`, `Mipmaps`, `Mrt`, `Msaa`, `MsaaCounts`, `MsaaShader`, `Piece2Bar`, `PieceAll`,
`PieceAtomic`, `PieceBits`, `PieceDev`, `PieceLoop`, `PiecePadPred`,
`PiecePadUncond`, `PieceRet`, `PieceShared`, `PieceSink`, `PieceSlow64`,
`PieceSlow`, `PieceUav`, `PieceWarp0`, `PieceWarp3`, `PieceWarp6`,
`Primitives`, `Query`, `RawBuffer`, `RenderTarget`, `Scissor`,
`ShadowCompare`, `Stencil`, `Tessellation`, `TexArray`, `TexOps`,
`Texture3d`, `Texture`, `TypedUav`, `Viewports`, `WireframeColor`,
`Wireframe`, `WireframeTess`.

Grouping chosen for implementation (each group is one C++ source file):

| Group | Scenes | DXVM relevance |
|---|---|---|
| `device` | device creation, format queries, capability probe (`MsaaCounts` sample-count enumeration) | feature override table, vendor helper |
| `state` | Blend, BlendFactor, BlendPrims, Depth, DepthClip, DepthClamp, DualSource, LogicOp, Scissor, Viewports, Wireframe*, FrontFace, CullDistance, Stencil, Msaa, Primitives, RenderTarget, Discard*, ErrCull, ErrClamp | state emulation module |
| `texture` | Texture, TexOps, TexArray, Texture3d, Cube, Mipmaps, Formats, Border, GatherOffset, ShadowCompare, MapNoOverwrite, RawBuffer, TypedUav | BCn remap + transcoder, format policy |
| `compute` | Compute, Atomics, Append, GroupShared, Ballot32*, Piece*, DispatchIndirect, AsyncCompile | FP16/INT8 options, shader options |
| `draw` | DrawIndirect, Instancing, Geometry, Tessellation, Query, Copy, Alu, InternalPasses, ErrStrides | general DXVK behaviour |
| `d3d9` | D3D9 device + its scene suite (~20 scenes reused from above via D3D9 API) | D3D9 path, vendor reporting |
| `d3d10` | D3D10 device + core scenes (RenderTarget, Texture, Blend, Depth) | D3D10 path |

Each scene function follows one signature:
`SceneResult run(const Device& dev, const Env& env)`, where `Env` carries parsed
switches. A scene returns `PASS`, `FAIL(reason)`, `SKIP(reason)`, or throws;
the runner converts exceptions to `CRASH`.

## Approaches considered

1. **Full matrix reproduction (chosen).** Port all 67 scenes from the observed
   DXML binary behaviour into C++ harness sources. Highest fidelity and produces
   the log format the owner already uses. Cost: large implementation and review
   surface; many scenes must be re-derived from black-box strings/screenshots
   rather than source.
2. **DXVM-focused harness.** Only test the modules DXVM changes. Cheaper and
   faster, but owner rejected it — the log would not be comparable to DXML
   reports.
3. **Wine script wrapper only.** Cheapest, no scene coverage; rejected.

## How it gets made (decisions already made)

- Language/platform: C++17 console exe, MinGW-w64 cross build with a new
  `meson.build` under `tests/dxvm-harness/`, output to `dist/win64/dxvm-harness.exe`
  (and win32 where possible).
- **Log format: taken verbatim from a real DXML report**
  (`DXML-testpack-Infinix_X6857-20261121-0453.txt`, a Mali-G615 MC2 r44p1 run),
  not guessed. Header:

  ```
  DXVM Test Pack report, <YYYY-MM-DD HH:MM>
  Harness: <version> (code <n>)
  Total: <N> passed, <M> failed or crashed

  <VENDOR> <model or "Wine/Proton host">
  Chip: <cpu>
  GPU: <gpu name>, driver <driver version>
  Vulkan <apiVersion>
  DXVM: <version of the loaded DLLs>
  ```

  Section header: `== <section> ==`. Result lines:

  ```
  PASS <name padded to 15, min 1 space><detail>
  FAIL <name padded to 15, min 1 space><detail>
  SKIP <name padded to 15, min 1 space><reason>
  CRASH <name padded to 15, min 1 space><detail>
  ```

  The detail text carries measured values against expectations in DXML's voice,
  e.g. `pixel (10,20) is 42,82, expected 42,82` and
  `reported, created, drawn and resolved: 64 partial pixels, desc samples 4`.
  Device line before results: `device created, feature level 0xb000`.
  Section summary: `SUMMARY <N> passed, <M> failed`.
  Diagnostics are indented three spaces. Runner prints
  `== How each scenario ended ==` with one line per backend.

  `CRASH` replaces DXML's crash notation because a Windows access violation
  yields an exit code rather than DXML's Android signal line.
  No `SKIP` appeared in the reference run, but the line kind is reserved for
  scenes not yet ported.
- Registry: one `SceneRegistry.cpp` with name → function pointer; `--scenes`
  comma list filters by exact name.
- Env: `--env-file PATH` parses `KEY=VALUE`; `--set KEY=VALUE` repeats; values
  are set with `_putenv_s` before device creation.
- Backend selection: `--backend raw|wrapper|forced`. `raw` leaves Vulkan ICD
  alone; `wrapper` sets `VK_ICD_FILENAMES` only if the owner supplies it in the
  env file (we do not ship a Windows Vulkan wrapper); `forced` is a synonym of
  `wrapper` for DXVM (no forced-start equivalent on Windows).
- Report file: `--out PATH` (default `dxvm-report.txt`) in addition to stdout.
- Wine launcher: `tests/dxvm-harness/run-wine.sh` sets `WINEDLLOVERRIDES` for
  `d3d11,d3d10core,d3d9,dxgi=n`, runs the exe, tees stdout to the report path.

## Left to the builder (implementation plan)

- Exact per-scene HLSL/D3D calls, since the Android binary gives only strings
  and behaviour, not source. Use the standard DXVK test style (create device,
  create resources, draw/compute, readback, compare).
- Which scenes need actual pixel readback vs API-call success. Default: readback
  for `texture`, `state`, `d3d9`; API success for `compute`, `draw` where
  readback would require large shaders.
- Whether D3D10 uses the D3D11 device's `ID3D10Device` interface or a separate
  `D3D10CreateDevice` call — prefer separate call through dxgi/d3d10core.
- How many of the 67 scenes can be implemented faithfully in one pass; report
  any that must be stubbed as `SKIP: not ported yet` rather than silently
  dropping them.

## Latest-build delta (MSAA)

The newest DXML Test Pack build (SHA-256 `199dc40d...`) adds exactly two scenes
over the build the list above was first read from:

- **`MsaaCounts`** — enumerate sample counts with
  `ID3D11Device::CheckMultisampleQualityLevels`, then attempt to create and
  resolve a texture at each reported count; fail when the device reports a count
  it cannot actually create. (Observed strings: `CheckMultisampleQualityLevels
  failed 0x%08x`, `4x MSAA not supported`, `reported, created, drawn and
  resolved: %d partial pixels, desc samples %u`.)
- **`MsaaShader`** — render a 4x MSAA triangle with partial coverage on a
  diagonal edge, read `SV_SampleIndex` / `Texture2DMS`, then resolve; assert the
  diagonal edge produces partial-coverage pixels. (Observed strings: `4x MSAA
  resolve, partial coverage on the edge`, `4x MSAA triangle resolved, diagonal
  edge has partial coverage`, `msaa%u.position`, `msaa%u.sampleindex`,
  `msaa4.texture2dms`.)

Both are in the `state` group; `MsaaCounts` is additionally a capability probe and
appears in the `device` group. The bundled stock DXVK 1.9.2 / 2.7.1 / 3.1.1
comparison libraries are byte-identical between builds, so the three-backend
comparison design is unaffected.

## Evidence from a real DXML run (Mali-G615 MC2)

A real report from the owner's phone (`Infinix X6857`, Mali-G615 MC2 driver
r44p1, Test Pack 1.49 code 51) shows DXML's own engine passing every MSAA test
while **stock DXVK 3.1.1 fails exactly two**:

```
== DXVK 3.1.1 on the Mali wrapper ==
-- Direct3D 11: 90 passed, 2 failed --
FAIL msaa2.sampleindex red is 26, expected 9
FAIL msaa16.sampleindex red is 128  ->  observed 26
SUMMARY 90 passed, 2 failed
```

The same two tests fail for DXVK 2.7.1 and 1.9.2, so this is a long-standing
upstream MSAA sample-index defect on Mali. **DXVM forked from DXVK 3.1.1 and
very likely inherits it.** `msaa4.texture2dms` passes, so the defect is in the
`SV_SampleIndex` ramp rather than in `Texture2DMS` reads.

Reference values the harness must assert (from DXML's passing runs on this GPU):

| Test | Expected |
|---|---|
| `msaa2.sampleindex` | `9` |
| `msaa4.sampleindex` | `26` |
| `msaa8.sampleindex` | `60` |
| `msaa16.sampleindex` | `128` |
| `msaa*.position` | pixel (10,20) = `42,82` |
| `msaa4.texture2dms` | `128,64,255` |
| `msaa*.*` coverage | 64 partial pixels |

These make `MsaaCounts` and `MsaaShader` the highest-value scenes in the whole
harness for DXVM: they are the only tests in the reference run where our fork
is expected to fail and DXML's own engine passes.

## Builder rulings (settled without owner input)

These were open questions from the builder check; each is decided here so the
plan can proceed:

- **Crash containment:** one subprocess per scene. The parent harness launches a
  child `dxvm-harness --child <scene>` with inherited stdout redirected to a
  per-scene temp file; a non-zero exit or missing `PASS` line becomes
  `CRASH <scene>: exit <code>`. This guarantees a line for every scene even
  after an access violation, which in-process SEH cannot reliably do under Wine.
- **Run scope:** one invocation runs the full matrix (all three APIs × all
  requested backends) and produces one report with `== ` blocks per
  API/backend, mirroring DXML's report. `M` in the summary is the number of
  scenes actually attempted in that run (not the global 73).
- **PASS oracle:** a scene passes when its own readback/API assertions succeed;
  there is no golden-image comparison against DXVK. The report is meant to be
  comparable in *shape*, not in pixel values. Every scene documents its oracle
  in a one-line comment above its registry entry.
- **DXVM version header:** read the file version of the actually loaded DLLs
  (`GetFileVersionInfo` on `d3d11.dll`), falling back to the build-time
  `version.h` string if unavailable. This keeps baseline runs with upstream
  DXVK correctly labelled.
- **Proton:** supported through the same launcher with
  `PROTON_DXVK_DLL` conventions; no separate code path in the harness itself.
- **D3D9/D3D10 scene lists:** the exact per-API scene subsets are left to the
  implementation plan; d3d9 and d3d10 groups reuse scene logic from the shared
  scene implementations where the API allows.

## Verification

- Build win64 + win32 with Meson.
- Run `dxvm-harness.exe --api d3d11 --scenes device,texture --out r.txt` under
  Wine with DXVM DLLs; confirm report has only `PASS`/`SKIP` lines for those
  groups and that `r.txt` matches stdout.
- Run the same with upstream DXVK DLLs in a separate prefix and diff the report
  shape (scene names/order) — not the pass/fail values.
- Full matrix run under Wine produces a single report file with all 73 names
  present as `PASS`, `FAIL`, `SKIP` or `CRASH`.
- `MsaaCounts` and `MsaaShader` are exercised explicitly on Mali hardware and the
  four `sampleindex` ramp values above are asserted exactly,
  since MSAA sample-count reporting is exactly the kind of mismatch the two
  new scenes were written to catch.
