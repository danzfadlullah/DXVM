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

## Deliberately not adopted from upstream

- `vkd3d-proton` and the audio components: not needed by a Direct3D 8/9/10/11 test app.
- The bundled PanVK driver remains available only as an **explicitly selected comparison**.
  The default and headline configuration is the phone's **system** driver, because that is
  the configuration DXVM exists to make viable.