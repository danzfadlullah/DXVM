# Owner run — DXVM TestPack

What this app is for: measuring DXVM against the phone's **own** Vulkan driver, with no
side-loaded driver. One Start button, one text report to paste back.

## 1. Install

Download `dxvm-testpack-debug.apk` from the latest `testpack` workflow run (Actions → testpack →
run → Artifacts → `dxvm-testpack-debug`).

The build is the debug build signed with the debug key, which is what keeps in-place upgrades
working. Uninstall any earlier TestPack build first only if the signature ever changes.

## 2. First start

Open **Components**. The APK carries:

| Component | Bundled | Why |
|---|---|---|
| Rootfs (`imagefs_bionic`) | yes | the Wine userland |
| Wine (`proton-11.0-2-arm64ec`) | yes | the engine |
| **DXVM** (`dxvm-0.1.0-alpha.1-arm64ec`) | yes | the D3D implementation under test |
| FEXCore | **no** | only needed for 32-bit (WoW64) Windows programs; the bundled harness is ARM64EC and runs natively |
| vkd3d-proton, Box64, audio | **no** | not used by a Direct3D 8/9/10/11 test app |

Everything bundled installs itself on first start. Wait for **Components** to stop showing
progress before pressing Start.

If a component fails to install, its row says so — fix that before running. A run with a missing
Rootfs or Wine cannot produce a report worth reading.

## 3. Keep the system driver selected

**Drivers**: leave the phone's own driver selected. Do **not** select PanVK for the normal run —
the whole point is the system driver. PanVK appears only in the optional comparison.

## 4. Press Start

The **Test** tab → **Start**. First run takes a few minutes: the Wine prefix is created once, then
DXVM's Direct3D suite launches inside it. The progress list names every step, so a stall is
visible rather than silent.

You will see, in order:

```
DXVM component: <version>              ← the component with our marker is installed
Driver: system, <GPU>, driver <ver>    ← read from the driver itself, not a setting
Direct3D suite: launching dxvm-harness inside the container
Vulkan suite: asking the driver N questions
Report: <n> passed, <m> failed or crashed
```

## 5. Send the report

**Share report** hands off the plain text. It is also written to the app's files directory as
`dxvm-testpack-report.txt`.

Read the header before sending: if it does not say `Driver: system`, the run did not measure what
this project needs, and the numbers describe a different driver. If it says
`NOTE: nothing ran`, the container or the component failed — that is the finding, not a clean run.

## Optional: comparison run

Only meaningful if you have installed **both** a PanVK driver (Drivers screen) and a stock DXVK
component (Components screen). The button then produces:

```
== Run A: system driver + DXVM ==
...
== Run B: PanVK + stock DXVK ==
...
```

Without both parts installed, Run B's section says exactly what is missing instead of pretending
to have run.

## What the report means for the next DXVM round

- `FAIL vk.bc` → the system driver cannot sample BCn at all, so DXVM's remap/transcode is
  required (it is not wasted work).
- `FAIL vk.blend-dual`, `vk.fill-mode`, `vk.multi-viewport`, `vk.blend-logic` → DXVM's state
  clamping is still needed on this driver.
- `FAIL`/`CRASH` in the Direct3D section names the exact scene, which is the unit of the next fix.
- `SKIP` is explicit and carries its reason; a missing line would be a bug.