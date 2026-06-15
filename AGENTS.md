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
  `rtk git status`, `rtk openspec validate --all`.

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

For documentation-only work, OpenSpec validation and git status checks are often
enough. Run the firmware build when the user asks for project status, build
confidence, or a pre-implementation baseline.

## Current Handoff State

This branch was started by Claude Code and is now maintained through Codex.
`CLAUDE.md` remains historical guidance, but this `AGENTS.md` is the Codex
entrypoint.

The OpenSpec change `port-firmware-esp32s31` is complete and archived:

- Archived planning artifacts are under
  `openspec/changes/archive/2026-06-15-port-firmware-esp32s31/`.
- Main specs are synced under `openspec/specs/`.
- `openspec list --json` should show no active changes for this completed pass.
- `openspec validate --all` should validate the seven synced main specs.
- `decision.md` records the resolved Group 6 audio decision: build an audio
  scaffold now, defer live UAC/Opus runtime work to hardware bring-up.
- `docs/COMPLETION.md` records the build-verified migration scaffold and the
  remaining hardware-only validation items.

Implementation state from the archived OpenSpec tasks:

- Groups 0-8 are marked complete.
- Group 6 audio is implemented as a default-disabled scaffold in
  `components/audio/`; do not vendor `xiph/opus` or WDL, do not inject TinyUSB
  UAC, and do not change the HID-only USB descriptor before hardware bring-up.
- Group 7 battery LED support is implemented in `components/battery_led/` as a
  plain GPIO path with `DS5_BATTERY_LED_GPIO=-1` disabled by default.
- Group 8 CI/docs/report work is complete; the firmware workflow uses the GHCR
  S31 image and emits firmware artifacts.

Do not claim runtime BT/USB/audio success until the paths are tested on physical
ESP32-S31 hardware.

## OpenSpec / Codex Workflow

Use the repo-local Codex skills in `.codex/skills/openspec-*` for OpenSpec work.
The `.claude/` helper directory may exist side-by-side for historical context and
should not be deleted unless the user explicitly requests cleanup.

Prefer these checks before resuming work:

```bash
rtk openspec list --json
rtk openspec validate --all
rtk git status --short --untracked-files=all
rtk diff -qr .claude/skills .codex/skills
```

When future development resumes, follow `docs/MIGRATION.md` and the OpenSpec task
hardware bring-up list. Keep `main/` thin and put each ported subsystem behind
its ESP-IDF component under `components/<name>/`.
