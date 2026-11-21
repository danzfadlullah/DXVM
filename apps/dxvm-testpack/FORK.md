# DXVM TestPack — fork provenance

This directory is a vendored fork of [PanPlay](https://github.com/zenithblue-oss/panplay),
reduced to a test shell and re-pointed at DXVM.

## Upstream

| Field | Value |
|---|---|
| Repository | `zenithblue-oss/panplay` |
| Commit | `469b997a061b` (2026-10-12T17:59:56Z) |
| Cloned with | `git clone --depth 1` |

## What this fork changes

1. **Engine kept as-is.** Wine/Proton arm64ec + FEX + the built-in X server, container/prefix
   management, component install/unpack, driver manager and session logs are upstream code,
   unmodified unless a task says otherwise.
2. **Pruned.** `tests/results/` (287 MB of upstream run screenshots and logs), `docs/` images
   and the nested `.git` were removed; 580 MB → 28 MB. Nothing the APK build needs was
   dropped. Upstream history still holds the removed evidence.
3. **Launcher surface stripped** (see the implementation plan, Task 3): games library,
   shortcuts, controller editor/overlay/input, audio, gamepad bridge, perf recorder, store.
4. **DXVM replaces DXVK.** A `type: "DXVK"` component built from this repo's DXVM source
   (arm64ec `system32/` + i686 `syswow64/`) takes the place of
   `dxvk-3.1.1-4-arm64ec.wcp`.
5. **Vulkan capability suite added**, ported from PanProbe and pointed at the **system** ICD.
6. **Report added**, in DXML Test Pack's format.

## Licences and obligations

| Part | Licence | Obligation |
|---|---|---|
| PanPlay's own code (this fork's base) | MIT (`LICENSE`) | keep the MIT notice |
| Built-in X server: `app/src/main/java/com/winlator/**`, `app/src/main/cpp/winlator/**` | **LGPL-2.1** (`third_party/winlator/LICENSE`) | must stay LGPL; do not delete or re-license; notices intact |
| Bundled components (Proton arm64ec, FEX, imagefs) | as `app/src/main/assets/components-NOTICE.txt` records | keep `NOTICE.md` and that file intact |
| PanProbe test sources (ported) | MIT | attributed in `NOTICE.md` |
| DXML Test Pack | **no licence published** | its **report format and scene names only** are mirrored as observable facts; **no code is copied** |
| DXVM itself | zlib (upstream DXVK licence) | unchanged |

`tests/component/test_licences.py` asserts the LGPL and notice files are still present, so a
future stripping pass cannot quietly remove them.

## Bundling after this fork's changes

`app/bundled-components.json` now lists only what a Direct3D 8/9/10/11 test run needs and what
can be obtained reproducibly in CI: the rootfs and Proton (pinned URLs, sha256-checked) plus
DXVM, which CI builds from this repository's source and passes with `-PdxvmWcp`. PanPlay's own
`dxvk-3.1.1-4-arm64ec.wcp`, `vkd3d`, Box64 and the audio components are gone; PanVK is no longer
bundled, because the default configuration is the phone's own driver.

FEXCore is also not bundled. It is only needed to run 32-bit (x86) Windows programs under the
arm64ec Wine; the harness this app runs is an ARM64EC image and needs no x86_64 emulation. If a
future scene needs WoW64, build FEXCore with `scripts/build-fex-windows.sh` and add it to the
list again.

The pinned PanVK driver **is** still bundled (`assets/bundled-driver.json`, downloaded and
sha256-checked by `copyPanvkSo`), because the optional comparison run needs it as the baseline
column. It is never the default and never what the headline run measures: the container writes a
`VK_ICD_FILENAMES` override only for a named driver, and the test pass asks for
`ContainerManager.SYSTEM_DRIVER_ID`, which writes no override at all so the phone's own ICD is
used. `Driver: system` in a report is therefore backed by the run's own request, not by a
setting — and the two halves of the report each name the driver they used, because the Direct3D
half runs inside Wine while the Vulkan half runs in this app's process.

## Deliberately not adopted from upstream

- `vkd3d-proton` and the audio components: not needed by a Direct3D 8/9/10/11 test app.
- PanVK is bundled but is used **only** by the explicitly pressed comparison button, as the
  baseline column. The default and headline configuration is the phone's **system** driver —
  the run requests it explicitly — because that is the configuration DXVM exists to make viable.
- vortek (Winlator's Vulkan wrapper, LGPL-2.1) is deliberately **not** adopted: it is another
  compatibility layer between DXVM and the driver (format emulation, SPIR-V rewriting), so with
  it in front a PASS would no longer mean "the phone's own driver can do this", and its format
  emulation overlaps the BCn remap/transcode DXVM exists to provide. It stays the documented
  fallback if a run ever shows the container cannot reach the system loader at all.