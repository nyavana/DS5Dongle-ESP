# Codex Instructions

## Project Purpose

This worktree is the ESP-IDF migration of the Pico 2W DualSense dongle firmware
to the ESP32-S31. The device must act as both:

- a Bluetooth Classic BR/EDR HID host for the DualSense controller, and
- a USB HID device presenting itself as a DualSense to the host.

The ESP32-S31 target is required because it has both BT Classic and native USB-OTG.
The Pico 2W reference implementation is in `../master` and mirrored in
`legacy-pico/` for side-by-side porting.

## Shell Commands

- Always prefix shell commands with `rtk`.
- Use this worktree as the working directory for migration commands:
  `rtk git status`, `rtk git diff --check`.

## Toolchain And Build Gate

`esp32s31` is an ESP-IDF preview target. Use `--preview` when selecting the target.

Canonical build gate:

```bash
rtk docker run --rm -v "$PWD":/p -w /p espidf:s31 idf.py --preview set-target esp32s31 build
```

CI uses the published container image
`ghcr.io/nyavana/ds5dongle-esp32s31-idf:master`. If the firmware workflow cannot
pull it, run `.github/workflows/build-toolchain-image.yml` first to publish the
image.

Do not edit generated `sdkconfig`; edit `sdkconfig.defaults` for reproducible
configuration. `build/`, `managed_components/`, `dependencies.lock`, and generated
firmware artifacts are ignored.

For documentation-only work, diff and git status checks are often enough. Run
the firmware build when the user asks for project status, build confidence, or
a pre-implementation baseline.

## Current Handoff State

This branch was started by Claude Code and is now maintained through Codex.
`CLAUDE.md` remains historical guidance, but this `AGENTS.md` is the Codex
entrypoint.

The initial migration scaffold is complete. `decision.md` records the resolved
audio decision: build an audio scaffold now and defer live UAC/Opus runtime work
to hardware bring-up. `docs/COMPLETION.md` records the build-verified scope and
remaining hardware-only validation items.

Current implementation state:

- Audio is implemented as a default-disabled scaffold in
  `components/audio/`; do not vendor `xiph/opus` or WDL, do not inject TinyUSB
  UAC, and do not change the HID-only USB descriptor before hardware bring-up.
- Battery LED support is implemented in `components/battery_led/` as a
  plain GPIO path with `DS5_BATTERY_LED_GPIO=-1` disabled by default.
- CI/docs/report work is complete; the firmware workflow uses the GHCR
  S31 image and emits firmware artifacts.

Do not claim runtime BT/USB/audio success until the paths are tested on physical
ESP32-S31 hardware.

## Project Workflow

Use the Trellis task and specification material under the checkout root's
`.trellis/` directory for planning and implementation context. Keep durable
architecture and verification decisions in `docs/`.

Prefer these checks before resuming work:

```bash
rtk git status --short --untracked-files=all
rtk git diff --check
```

When future development resumes, follow `docs/MIGRATION.md` and the hardware
bring-up list in `docs/COMPLETION.md`. Keep `main/` thin and put each ported
subsystem behind its ESP-IDF component under `components/<name>/`.
