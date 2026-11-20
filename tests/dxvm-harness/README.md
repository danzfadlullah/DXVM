# DXVM test harness

A Windows console tool that runs Direct3D compatibility tests against **DXVM**
and writes a report in DXML's own format, so a run can be pasted back for the
next development round.

The scene names and the report layout come from the real DXML Test Pack: 73
scene functions were read out of its `libdxmlpack.so`, and the line format from
the owner's own report on a Mali-G615 MC2 (driver r44p1).

## Why Windows, not the Android APK

The DXML APK loads `ELF arm64` `.so` libraries. DXVM produces Windows PE DLLs
for Wine/Proton, so the APK cannot load it. This harness is the Windows-side
equivalent: same scene language, same report shape.

## Build

```bash
meson setup --cross-file build-win64.txt --buildtype release build-win64
meson compile -C build-win64 dxvm-harness
```

Produces `build-win64/tests/dxvm-harness/dxvm-harness.exe` (and the x86 build
under `build-win32`). The harness is Windows-only and is skipped automatically
for dxvk-native and Linux builds; enable it explicitly with `-Dharness=enabled`.

## Run under Wine

```bash
# DXVM DLLs beside the exe win over anything else on the DLL search path
bash tests/dxvm-harness/run-wine.sh --dll-dir dist/win64 --out dxvm-report.txt
```

Options: `--api d3d11,d3d10,d3d9`, `--backend raw|wrapper|forced`,
`--scenes a,b`, `--show`.

Running the exe directly works too; put the DXVM DLLs next to it and set
`WINEDLLOVERRIDES=d3d8,d3d9,d3d10core,d3d11,dxgi=n`.

## Reading the report

```
DXVM Test Pack report, 2026-11-21 06:40
Harness: 0.1.0 (code 1)
Total: 31 passed, 1 failed or crashed

GPU: Mali-G615 ..., driver 13b5:...

== Direct3D 11 suite (raw driver) ==
PASS device.create      feature level 0xb000
FAIL msaa.sampleindex   red is 0, expected 25 (same index for every sample?)
SKIP compute            not ported yet
...
SUMMARY 31 passed, 1 failed
```

- `PASS` — the scene's own assertions held.
- `FAIL` — measured values differ from the expectation; the detail names both.
- `SKIP` — the scene could not run (no shader compiler, format unsupported) or
  is not ported yet. **A SKIP is never a pass**; it is missing evidence.
- `CRASH` — the scene killed its process. The harness runs every scene in its
  own child process, so one crash cannot truncate the rest of the report.

Send the whole file. It contains no personal data — only GPU/driver strings and
test results.

## What is actually tested today

Real end-to-end tests:

| Scene | What it proves | DXVM module it exercises |
|---|---|---|
| `device.create`, `device.featurelevels`, `device.formats` | device exists, level ≥ 11_0, required formats supportable | feature override table |
| `msaacounts` | every sample count the driver reports is also creatable | feature/property reporting |
| `msaa.coverage` | 4x MSAA resolve has 64 partial-coverage pixels on the diagonal | MSAA raster state |
| `msaa.sampleindex` | resolved red equals the CPU mean of per-sample values | sample-index mapping |
| `msaa.reference` | prints DXML's reference values (2x=9, 4x=26, 8x=60, 16x=128) | comparison aid |
| `logicop`, `dualsource`, `wireframe`, `viewports` | emulated state still renders the expected colour | `dxvm_mali_state_emulation` |
| `texture` | a 4x4 texture samples non-black | upload path |
| `bcn.bc1`, `bcn.bc1_srgb`, `bcn.bc3`, `bcn.bc3_srgb` | a solid-red BCn block samples red, i.e. remap + transcode worked | `dxvm_mali_format_policy`, BCn transcoder |
| `d3d9.device`, `d3d9.msaa` | D3D9 device creates; multisample types enumerated | D3D9 path, vendor reporting |
| `d3d10.create`, `d3d10.level101` | `d3d10core.dll` loads and creates a device ≥ 10_1 | D3D10 path |

Registered but reporting `SKIP: not ported yet`: the compute and warp family
(`piece*`, `ballot32`, `groupshared`, …), geometry/tessellation, indirect draws,
mipmap/array/cube texture scenes, and the remaining BCn formats (BC2, BC4, BC5,
BC6H, BC7). They are listed on purpose so the report always accounts for every
planned scene instead of silently omitting it.

## Known issue this harness is expected to expose

Stock DXVK 3.1.1 — the version DXVM forks — fails two MSAA sample-index tests on
Mali. From the owner's DXML run:

```
FAIL msaa2.sampleindex red is 26, expected 9
FAIL msaa16.sampleindex red is 26, expected 128
```

Both fail for DXVK 2.7.1 and 1.9.2 as well, while DXML's own engine passes, and
`msaa4.texture2dms` passes — so the defect is in the `SV_SampleIndex` ramp, not
in `Texture2DMS` reads. **A failing `msaa.sampleindex` on DXVM is the expected
first finding, not a harness bug.**

## Host-side unit tests

The report writer, scene registry, environment parsing, crash containment, and
the pinned MSAA reference table are pure C++ and run without Wine:

```bash
bash tests/dxvm-harness/tests/run.sh all
```