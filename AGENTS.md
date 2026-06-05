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
  `rtk git status`, `rtk openspec status --change port-firmware-esp32s31 --json`.

## Toolchain And Build Gate

`esp32s31` is an ESP-IDF preview target. Use `--preview` when selecting the target.

Canonical build gate:

```bash
rtk docker run --rm -v "$PWD":/p -w /p espidf:s31 idf.py --preview set-target esp32s31 build
```

Do not edit generated `sdkconfig`; edit `sdkconfig.defaults` for reproducible
configuration. `build/`, `managed_components/`, `dependencies.lock`, and generated
firmware artifacts are ignored.

For a documentation-only handoff/init, do not run the firmware build unless the
user explicitly asks. Use OpenSpec and git status checks instead.

## Current Handoff State

This branch was started by Claude Code and is being handed to Codex. `CLAUDE.md`
is still useful historical guidance, but this `AGENTS.md` is the Codex entrypoint.

The active OpenSpec change is `port-firmware-esp32s31`:

- Planning artifacts are under `openspec/changes/port-firmware-esp32s31/`.
- `openspec status --change port-firmware-esp32s31 --json` should report
  `isComplete: true` for proposal, design, specs, and tasks.
- `decision.md` captures the paused Claude decision about Group 6 audio.

Implementation state from `openspec/changes/port-firmware-esp32s31/tasks.md`:

- Groups 0-4 are marked complete.
- Group 5 is complete except `disable_pico_led`, which is deferred to the battery
  indicator work.
- Group 6 audio is intentionally paused pending a user decision about stubbing,
  full vendoring/UAC work, or skipping to cleanup.
- Groups 7-8 remain pending.

Do not continue Groups 6-8 or answer the audio decision during project init.

## OpenSpec / Codex Workflow

Use the repo-local Codex skills in `.codex/skills/openspec-*` for OpenSpec work.
The `.claude/` helper directory may exist side-by-side for historical context and
should not be deleted unless the user explicitly requests cleanup.

Prefer these checks before resuming work:

```bash
rtk openspec status --change port-firmware-esp32s31 --json
rtk git status --short --untracked-files=all
rtk diff -qr .claude/skills .codex/skills
```

When future development resumes, follow `docs/MIGRATION.md` and the OpenSpec task
order. Keep `main/` thin and put each ported subsystem behind its ESP-IDF component
under `components/<name>/`.
