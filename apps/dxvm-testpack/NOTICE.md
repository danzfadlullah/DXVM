# NOTICE

This repository mixes license regimes. Do not treat the tree as single-license MIT.
Split from `apps/panvk-launcher` of https://github.com/zenithblue-oss/panvk-kbase-android
(see that repository's `NOTICE.md` for the full driver-side notice).

## 1. MIT: PanPlay's own code

Launcher UI, scripts, tests, packaging and docs authored for this project are MIT-licensed
unless a file header states otherwise. See `LICENSE`.

## 2. Winlator X server: vendored, LGPL-2.1

The built-in X server is copied and modified from https://github.com/brunodev85/winlator at commit
`db6d7aa446b5607b48ac2ee36dc4333799ff3dc7` (brunodev85 and Winlator contributors), licensed
**LGPL-2.1**; license text in `third_party/winlator/LICENSE`.

Vendored locations:

- `app/src/main/java/com/winlator/`
- `app/src/main/cpp/winlator/`
- `third_party/winlator/` (license and README with the file list and modifications; changes are
  marked `panvk-launcher` in the sources)

These files stay LGPL-2.1 and are not relicensed. Full source is in this repository so recipients can
modify the code and rebuild the APK.

## 3. Bundled components (not vendored in git)

Proton/Wine, imagefs, FEX, DXVK and the PanVK driver are bundled into release APKs with their own
licenses. Sources, versions and hashes: `app/src/main/assets/components-NOTICE.txt`,
`scripts/components.env`, `app/bundled-components.json`, `app/src/main/assets/bundled-driver.json`.
The PanVK driver is Mesa-derived (MIT); its source and notices are in
https://github.com/zenithblue-oss/panvk-kbase-android.

## 4. PanProbe test sources: vendored at compile time, MIT

The Vulkan capability suite in `app/src/main/cpp/vktests/` is derived from
[PanProbe](https://github.com/abhay-byte/panprobe)'s `tests/dxvk/vulkan/` suite
(dual-source blend, logic-op blend, MRT blend, fill-mode, multi-viewport, BCn sampling,
DXVK feature requirements), which is **MIT**-licensed. Copyright (c) the PanProbe authors.

This fork re-implements those tests as headless capability queries against the selected
ICD rather than as rendering suites; the ported set and the deviation are recorded in
`docs/superpowers/specs/2026-11-21-dxvm-testpack-android-design.md`. PanProbe's MIT
notice applies to any source derived from it. The MIT text is in `LICENSE` (kept
alongside this notice so the attribution travels with the APK).

## 5. DXVM itself: zlib (DXVK upstream licence)

DXVM is a fork of [DXVK](https://github.com/doitsujin/dxvk) and carries DXVK's zlib
licence; it is built from this repository's root and packaged as
`dxvm-<version>-arm64ec.wcp` by `tools/build-dxvm-component.sh`. The Windows harness
(`dxvm-harness.exe`, `tests/dxvm-harness/`) is part of that same tree.

## 6. DXML Test Pack: no licence published

DXML Test Pack publishes no licence. **No DXML code is copied into this repository.**
Only its observable output format (report line kinds, section headers, summary wording,
the totals table shape) and its scene names are mirrored, because those are facts about
its output rather than its implementation. Nothing here is derived from DXML source.
