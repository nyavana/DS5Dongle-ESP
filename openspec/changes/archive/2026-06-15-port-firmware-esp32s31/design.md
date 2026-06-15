## Context

The shipping firmware (`master` worktree) is a single-super-loop Pico 2W build: `main()` sets clocks, brings up TinyUSB (device), CYW43+BTstack (BR/EDR HID host), and then spins a loop calling `cyw43_arch_poll` / `tud_task` / `audio_loop` / `interrupt_loop`. It bridges a DualSense (BT Classic HID) to a console (USB HID device), relaying input/output reports and tunnelling haptics + speaker audio.

The `esp32-s31` branch now has the controller path building on ESP-IDF: boot, USB HID, BR/EDR HID host, report bridge, config, command, state manager, and report buffer are ported. This design covers the remaining subsystem work on the **`esp32s31` preview target** (BT 5.4 Classic + USB-OTG). The reference sources are mirrored read-only in `legacy-pico/src/`; the module mapping is in `docs/MIGRATION.md`.

Two hard constraints shape every decision:
1. **Autonomous execution** — the agent works without human input, self-fixes, and only files a report at the end.
2. **Toolchain & no hardware** — only the `espidf:s31` (ESP-IDF `master`) Docker image is available for verification; there is **no physical S31**. So "verified" = clean target build + faithful logical port; runtime BT/USB/audio behaviour cannot be exercised.

## Goals / Non-Goals

**Goals:**
- Port all seven subsystems (boot, usb-device, bt-hid-host, report-bridge, device-config, audio-bridge, battery-indicator) into ESP-IDF components under `components/<name>/`, each behind the *existing* Pico module header (`bt.h`, `usb.h`, `audio.h`, `state_mgr.h`, `cmd.h`, `config.h`, `battery_led.h`) so the cross-module contracts and `main/` glue stay stable.
- Replace the super-loop with `app_main()` + FreeRTOS tasks/callbacks; keep `main/` thin.
- Keep externally-visible behaviour identical: same USB descriptors/VID-PID, same DualSense report formats (`0x01`/`0x31`/`0x36`), same `0xf6`–`0xf9` command protocol, same config semantics.
- A clean `idf.py --preview set-target esp32s31 build` after every subsystem lands.

**Non-Goals:**
- On-hardware bring-up, timing/performance tuning, or power optimisation (deferred to human, flagged in the report).
- Supporting any target other than `esp32s31`; no BLE path.
- Changing the wire protocol, descriptor layout, or config schema.
- Reworking the audio algorithm — the audio scaffold preserves the existing resample/Opus/`0x36` packet shape, but runtime UAC capture and Opus transmission are deferred until S31 hardware can validate them.

## Decisions

### D1 — BT HID host: Bluedroid `esp_hidh` (BR/EDR) first, raw L2CAP as fallback
The Pico `bt.cpp` is a hand-rolled HID host over **raw BTstack L2CAP** (manual inquiry by class-of-device, PIN/link-key/user-confirmation pairing, explicit channels on PSM `0x11`/`0x13`). Two ESP options:
- **(chosen) `esp_hidh` Bluedroid BR/EDR HID host** — manages L2CAP HID channels, pairing, bonding, reconnection, and exposes input-report callbacks + output-report send. Maps cleanly onto `bt_register_data_callback` / `bt_write`, and bonding persists in NVS for free.
- **Raw L2CAP via `esp_bt`/Bluedroid L2CAP API** — a closer 1:1 to the Pico code but re-implements pairing/discovery by hand.

Rationale: `esp_hidh` is the smaller, more maintainable surface and the DualSense input/output reports (`0x31`/`0x36`) are ordinary HID reports the profile can carry. The buildable port uses `esp_hidh` and preserves the unchanged `bt.h` interface. **Mitigation for preview-target risk:** if hardware testing shows `esp_hidh` on S31 cannot deliver real DualSense report traffic reliably, swap the component internals to raw Bluedroid L2CAP without touching callers.

### D2 — USB via `esp_tinyusb` with explicit (ported) descriptors
TinyUSB sits under both builds, so `usb_descriptors.cpp` / `tusb_config.h` port closely. Decision: vendor the descriptors into `components/usb` and feed them to `esp_tinyusb` as **custom descriptors** (not auto-generated), porting the `tud_*` callbacks. The autonomous build keeps **HID-only enumeration** as the supported target. The Pico **HID + UAC audio** descriptor layout remains the reference, but UAC enablement is deferred behind an explicit follow-up because `esp_tinyusb` exposes HID configuration here but not a supported audio-class Kconfig/API path.

### D3 — Concurrency: FreeRTOS tasks + `portMUX`, dual-core for Opus
- The super-loop dissolves into: Bluedroid host callbacks (input reports), the `esp_tinyusb` device task (USB), and a dedicated audio task.
- The shared `interrupt_in_data` buffer + `report_dirty` flag move from `critical_section_t` to `portMUX_TYPE` / `taskENTER_CRITICAL`.
- For the audio scaffold, the Pico `multicore_launch_core1` shape becomes a FreeRTOS task/queue/`portMUX` boundary, but Opus processing stays default-disabled until UAC input and hardware timing can be tested.
- Watchdog: `esp_task_wdt` replaces `hardware/watchdog.h`.

### D4 — Config & bonding in NVS
`Config` persists as a single NVS blob (namespace `ds5`, key `config`) instead of a raw flash sector; the existing magic/version/size/CRC validation is kept (belt-and-braces over NVS's own integrity). BR/EDR bonds are stored by Bluedroid in its own NVS namespace automatically. Drops `hardware/flash` / `hardware/sync`.

### D5 — Dependencies: managed where possible, vendored where not
- `esp_tinyusb` remains the managed USB wrapper for the HID controller path.
- Future Opus encode should use `espressif/esp_audio_codec^2.5.0`, which provides OPUS encoder support and ESP32-S31 compatibility; do **not** vendor `xiph/opus` in this migration pass.
- Do not vendor WDL in this migration pass. Keep haptics resampling behind an explicit TODO until UAC frames can be tested on hardware; the buildable scaffold may use placeholder haptics/speaker payload helpers.
- Use a plain ESP-IDF GPIO battery LED path by default with a compile-time disabled pin value (for example `DS5_BATTERY_LED_GPIO=-1`). Do not add `led_strip` until the actual board requires an addressable LED.

### D6 — Group 6 audio scope: buildable scaffold, runtime deferred
Audio is the one subsystem whose real inputs and outputs depend on both USB UAC enumeration and Bluetooth audio-report timing. The autonomous pass will create `components/audio`, preserve the `audio.h` API, implement the FreeRTOS/queue/`portMUX` structure, hold headset state, and keep the `0x36` packet construction logic close to the Pico source. It will not enable USB UAC capture, inject `CFG_TUD_AUDIO` into TinyUSB, or send live audio reports by default. The completion report must list the exact hardware follow-up: enable and validate HID+UAC composite enumeration, feed 4-channel host audio, enable `espressif/esp_audio_codec` Opus encode, then test haptics/speaker/headset routing on S31 hardware.

### D7 — Execution defaults: no additional questions
The remaining implementation can proceed without asking for user decisions:
- Group 6 uses the audio scaffold only: no UAC descriptors, no live Opus, no WDL vendoring, no speaker-active suppression.
- Group 7 uses the plain GPIO battery LED default with the LED disabled when no pin is configured.
- Group 8 uses the GHCR ESP-IDF S31 image for CI and reports hardware-dependent validation as deferred.

### D8 — Autonomous verification model: build-gate + self-fix loop
Each subsystem task ends with a target build through the Docker image:
`docker run --rm -v $PWD:/p -w /p espidf:s31 idf.py --preview set-target esp32s31 build`.
The agent iterates on compiler/linker errors until clean, commits per subsystem, and proceeds in porting order. Because no hardware exists, the spec scenarios that describe **runtime** behaviour are satisfied by faithful, reviewed translation — not execution — and every such untested path is enumerated in the final report. `sdkconfig.defaults` symbols (`CONFIG_BT_CLASSIC_ENABLED`, USB-OTG, CPU-freq, NVS, task-WDT) are treated as starting points and reconciled against what the S31 Kconfig actually exposes during the boot step.

## Risks / Trade-offs

- **`esp_hidh` may not support real DualSense traffic on S31 hardware despite building cleanly** → isolate BT behind `bt.h`; fall back to raw Bluedroid L2CAP (D1) with no caller changes if hardware bring-up proves it necessary.
- **`esp_tinyusb` composite HID+UAC may be immature on S31 USB-OTG** → keep HID-only as the buildable target (D2); UAC is a documented hardware follow-up, not an autonomous-build requirement.
- **ESP-IDF `master` API/Kconfig drift** → pin to the prebuilt `espidf:s31` image; keep each subsystem in its own component so an IDF change touches one place.
- **Opus vendoring risk** → avoid vendoring `xiph/opus`; use `espressif/esp_audio_codec^2.5.0` for the future runtime path (D5).
- **WDL resampler portability** → do not vendor WDL in this pass; keep resampling stubbed until hardware validation can exercise audio frames.
- **No hardware → runtime/timing/pairing bugs are uncatchable here** → faithful port + per-path "untested, needs hardware" callouts in the report; do not claim runtime success.
- **Memory: Opus encoder + audio/report buffers + dual BT/USB stacks on the S31** → caught at link time; if RAM-constrained, reduce Opus complexity/buffers (already minimal) before touching the protocol.
- **Audio path uses no I2S despite the doc's "Opus + I2S" note** — the real path is USB-UAC-in → resample/Opus → BT report `0x36`. The port follows the *code*, not the note; I2S is not introduced.

## Migration Plan

Port in the migration-doc order, each step isolated behind its existing header and gated by a clean target build + commit:
1. **boot** — confirm `esp32s31` target/clock/sdkconfig; `app_main` + task skeleton + logging + `esp_task_wdt`.
2. **usb-device** — `esp_tinyusb` + ported descriptors; enumerate as DualSense (HID-only first).
3. **bt-hid-host** — Bluedroid BR/EDR HID host (D1); deliver input reports to the bridge callback; `bt_write`.
4. **report-bridge + state_mgr + cmd** — input/output relay, polling-rate modes, `0x31` output build.
5. **device-config** — NVS store + `0xf6`–`0xf9` command interface.
6. **audio-bridge scaffold** — component/API, FreeRTOS queue/task shape, headset state, `0x36` packet construction helpers, and documented UAC/Opus hardware follow-up; no default-on UAC or live Opus path.
7. **battery-indicator** — plain GPIO LED path with disabled default pin + disconnect/staleness handling.

Also port `.github/workflows/` from the Pico `.uf2` build to `idf.py build`.

**Rollback:** the scaffold is preserved in git history; components are additive and independently revertible, and `legacy-pico/src` stays as the reference, so a broken subsystem can be reverted without disturbing the others.

## Hardware Bring-Up Deferrals

Resolved in the current build:
- S31 Kconfig symbols for CPU frequency, BT Classic/HID host, USB HID count, task watchdog, flash size, and partition table are recorded in `sdkconfig.defaults`.
- The implementation path is `esp_hidh` behind `bt.h`, not a raw-L2CAP first pass.
- The Group 6 scope is the default-disabled audio scaffold, not full UAC/Opus runtime.
- CI packaging uses a published GHCR toolchain image (`ghcr.io/nyavana/ds5dongle-esp32s31-idf:master`) built from ESP-IDF master, rather than rebuilding ESP-IDF inside every firmware job.
- The Group 7 default is a plain GPIO battery LED path with the pin disabled until the board pin is known.

Do not ask the user to decide these during implementation. Record them in the completion report for the later hardware session:
- Confirm `esp_hidh` on real S31 hardware delivers DualSense report traffic and reconnection behaviour reliably enough; if not, replace only the `bthost` internals with raw Bluedroid L2CAP.
- Confirm HID-only USB enumeration is accepted by the intended host as a DualSense-compatible controller.
- Confirm HID+UAC composite enumeration is viable through `esp_tinyusb`/TinyUSB on real S31 hardware before enabling runtime UAC.
- Confirm `espressif/esp_audio_codec^2.5.0` plus the chosen buffers can meet DualSense speaker packet timing once UAC input is available.
- Set the actual battery LED GPIO or replace the GPIO implementation with `led_strip` only if the chosen board requires it.
