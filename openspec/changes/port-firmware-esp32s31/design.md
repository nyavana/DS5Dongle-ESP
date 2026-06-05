## Context

The shipping firmware (`master` worktree) is a single-super-loop Pico 2W build: `main()` sets clocks, brings up TinyUSB (device), CYW43+BTstack (BR/EDR HID host), and then spins a loop calling `cyw43_arch_poll` / `tud_task` / `audio_loop` / `interrupt_loop`. It bridges a DualSense (BT Classic HID) to a console (USB HID device), relaying input/output reports and tunnelling haptics + speaker audio.

The `esp32-s31` branch has only a scaffold (`app_main` logs and idles). This design covers porting every subsystem to ESP-IDF on the **`esp32s31` preview target** (BT 5.4 Classic + USB-OTG). The reference sources are mirrored read-only in `legacy-pico/src/`; the module mapping is in `docs/MIGRATION.md`.

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
- Reworking the audio algorithm — the port mirrors the existing resample/Opus pipeline.

## Decisions

### D1 — BT HID host: Bluedroid `esp_hidh` (BR/EDR) first, raw L2CAP as fallback
The Pico `bt.cpp` is a hand-rolled HID host over **raw BTstack L2CAP** (manual inquiry by class-of-device, PIN/link-key/user-confirmation pairing, explicit channels on PSM `0x11`/`0x13`). Two ESP options:
- **(chosen) `esp_hidh` Bluedroid BR/EDR HID host** — manages L2CAP HID channels, pairing, bonding, reconnection, and exposes input-report callbacks + output-report send. Maps cleanly onto `bt_register_data_callback` / `bt_write`, and bonding persists in NVS for free.
- **Raw L2CAP via `esp_bt`/Bluedroid L2CAP API** — a closer 1:1 to the Pico code but re-implements pairing/discovery by hand.

Rationale: `esp_hidh` is the smaller, more maintainable surface and the DualSense input/output reports (`0x31`/`0x36`) are ordinary HID reports the profile can carry. **Mitigation for preview-target risk:** keep all of this behind the unchanged `bt.h` interface; if `esp_hidh` on the S31 preview can't deliver raw report passthrough or class-of-device-filtered discovery, swap the component internals to raw Bluedroid L2CAP without touching any caller. This is the single biggest unknown.

### D2 — USB via `esp_tinyusb` with explicit (ported) descriptors
TinyUSB sits under both builds, so `usb_descriptors.cpp` / `tusb_config.h` port closely. Decision: vendor the descriptors verbatim into `components/usb` and feed them to `esp_tinyusb` as **custom descriptors** (not auto-generated), porting the `tud_*` callbacks. The composite **HID + UAC audio** device is preserved. Land **HID-only enumeration first** (porting-order step 2), then add the UAC audio interfaces with the audio subsystem — so a UAC immaturity on the S31 USB-OTG stack doesn't block the controller path.

### D3 — Concurrency: FreeRTOS tasks + `portMUX`, dual-core for Opus
- The super-loop dissolves into: Bluedroid host callbacks (input reports), the `esp_tinyusb` device task (USB), and a dedicated audio task.
- The shared `interrupt_in_data` buffer + `report_dirty` flag move from `critical_section_t` to `portMUX_TYPE` / `taskENTER_CRITICAL`.
- The Pico `multicore_launch_core1` Opus encoder becomes a FreeRTOS task pinned to the app core; the Pico `queue_t` becomes a FreeRTOS queue; `opus_cs` becomes a `portMUX`.
- Watchdog: `esp_task_wdt` replaces `hardware/watchdog.h`.

### D4 — Config & bonding in NVS
`Config` persists as a single NVS blob (namespace `ds5`, key `config`) instead of a raw flash sector; the existing magic/version/size/CRC validation is kept (belt-and-braces over NVS's own integrity). BR/EDR bonds are stored by Bluedroid in its own NVS namespace automatically. Drops `hardware/flash` / `hardware/sync`.

### D5 — Dependencies: managed where possible, vendored where not
- `esp_tinyusb` and `opus` via `main/idf_component.yml` (ESP Component Registry); resolve the exact Opus package id/version during the audio step, vendoring Opus only if the registry entry is unusable.
- The WDL resampler has no IDF equivalent → **vendor** the minimal resampler into `components/audio` (as the migration doc anticipates). `led_strip` (managed) or plain GPIO for the battery indicator.

### D6 — Autonomous verification model: build-gate + self-fix loop
Each subsystem task ends with a target build through the Docker image:
`docker run --rm -v $PWD:/p -w /p espidf:s31 idf.py --preview set-target esp32s31 build`.
The agent iterates on compiler/linker errors until clean, commits per subsystem, and proceeds in porting order. Because no hardware exists, the spec scenarios that describe **runtime** behaviour are satisfied by faithful, reviewed translation — not execution — and every such untested path is enumerated in the final report. `sdkconfig.defaults` symbols (`CONFIG_BT_CLASSIC_ENABLED`, USB-OTG, CPU-freq, NVS, task-WDT) are treated as starting points and reconciled against what the S31 Kconfig actually exposes during the boot step.

## Risks / Trade-offs

- **`esp_hidh` may not support DualSense raw report passthrough / CoD-filtered discovery on the S31 preview** → isolate BT behind `bt.h`; fall back to raw Bluedroid L2CAP (D1) with no caller changes.
- **`esp_tinyusb` composite HID+UAC may be immature on S31 USB-OTG** → ship HID-only first (D2); audio interfaces are added later and can degrade gracefully if UAC isn't ready.
- **ESP-IDF `master` API/Kconfig drift** → pin to the prebuilt `espidf:s31` image; keep each subsystem in its own component so an IDF change touches one place.
- **Opus package id/version unknown; WDL resampler portability** → resolve registry id at the audio step, vendor as fallback (D5).
- **No hardware → runtime/timing/pairing bugs are uncatchable here** → faithful port + per-path "untested, needs hardware" callouts in the report; do not claim runtime success.
- **Memory: Opus encoder + audio/report buffers + dual BT/USB stacks on the S31** → caught at link time; if RAM-constrained, reduce Opus complexity/buffers (already minimal) before touching the protocol.
- **Audio path uses no I2S despite the doc's "Opus + I2S" note** — the real path is USB-UAC-in → resample/Opus → BT report `0x36`. The port follows the *code*, not the note; I2S is not introduced.

## Migration Plan

Port in the migration-doc order, each step isolated behind its existing header and gated by a clean target build + commit:
1. **boot** — confirm `esp32s31` target/clock/sdkconfig; `app_main` + task skeleton + logging + `esp_task_wdt`.
2. **usb-device** — `esp_tinyusb` + ported descriptors; enumerate as DualSense (HID-only first).
3. **bt-hid-host** — Bluedroid BR/EDR HID host (D1); deliver input reports to the bridge callback; `bt_write`.
4. **report-bridge + state_mgr + cmd** — input/output relay, polling-rate modes, `0x31` output build.
5. **device-config** — NVS store + `0xf6`–`0xf9` command interface; wire UAC audio interfaces into USB.
6. **audio-bridge** — Opus task + resampler + `0x36` packetisation.
7. **battery-indicator** — board LED/`led_strip` + disconnect/staleness handling.

Also port `.github/workflows/` from the Pico `.uf2` build to `idf.py build`.

**Rollback:** the scaffold is preserved in git history; components are additive and independently revertible, and `legacy-pico/src` stays as the reference, so a broken subsystem can be reverted without disturbing the others.

## Open Questions

- Does `esp_hidh` (BR/EDR) on the S31 preview expose the raw DualSense `0x31`/`0x36` reports and class-of-device-filtered discovery, or is raw L2CAP required? (Resolve in step 3.)
- Exact Opus ESP Component Registry package id/version. (Resolve in step 6.)
- S31 USB-OTG speed actually supported by `esp_tinyusb` (FS vs HS) and whether composite HID+UAC enumerates. (Resolve in steps 2/5.)
- Final `sdkconfig` symbol names for BR/EDR + CPU-freq on the S31 Kconfig (some carry chip-specific suffixes). (Resolve in step 1.)
