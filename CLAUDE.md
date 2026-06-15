# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An **ESP-IDF port to the ESP32-S31** of the Pico 2W DualSense bridge firmware.

**Status: build-verified ESP32-S31 migration scaffold** — boot, USB HID, BR/EDR
HID host, HID report bridge, NVS config, command handling, state management, the
shared report buffer, the default-disabled audio scaffold, battery LED support,
CI, and completion docs are ported and build clean. Real hardware bring-up remains
pending. The working/shipping firmware is still the Pico 2W build on the upstream
`master` branch until S31 hardware validation is complete.

The firmware (once ported) is a wireless **DualSense (PS5 / "DS5") adapter**:
simultaneously a **Bluetooth Classic (BR/EDR) HID _host_** to the controller and a
**USB HID _device_** posing as a DualSense to the host. That dual BR/EDR + USB-device
requirement is exactly why the port targets the **S31** — it's the only single-chip
ESP with both BT 5.4 Classic *and* USB-OTG (the S3 is BLE-only; the original ESP32
has no native USB device).

## Building & toolchain

`esp32s31` is a **preview target that only exists in ESP-IDF `master`** — no tagged
release or stock `espressif/idf` image has it yet (verified 2026-06). Hence the
`--preview` flag is required.

```bash
idf.py --preview set-target esp32s31
idf.py build
idf.py -p <PORT> flash monitor
```

Toolchain options:
- **Docker (recommended):** build the `espidf:s31` image from IDF `master` (exact
  `docker build` command in `README.md` / `docs/MIGRATION.md`).
- **Dev Container:** open the folder in VS Code Dev Containers — `.devcontainer/`
  uses that `espidf:s31` image.
- **Native:** clone ESP-IDF `master`, `./install.sh esp32s31`, `. ./export.sh`.

`sdkconfig` is generated and git-ignored — edit `sdkconfig.defaults` so the config
stays reproducible across worktrees / CI / the devcontainer. The current BT,
CPU-frequency, USB-HID, task-watchdog, flash-size, and partition symbols have been
reconciled against the local `espidf:s31` image; re-check them after any ESP-IDF
`master` update. Partition layout is in `partitions.csv`.

There is no unit-test suite — "verification" is a clean `idf.py build` plus
on-hardware testing.

## Migration model — read `docs/MIGRATION.md` first

`docs/MIGRATION.md` is the **authoritative module-by-module port plan and porting
order**; `components/README.md` has the component table. The approach:

- `legacy-pico/src/` is a **verbatim mirror of the Pico `master/src`** kept for
  side-by-side porting. Don't build it — it's reference.
- The Pico build's single super-loop becomes `main/main.cpp` `app_main()` + **FreeRTOS
  tasks driven by BT/USB event callbacks**. Keep `main/` thin.
- Each Pico module is ported into its own ESP-IDF **component** under
  `components/<name>/` (each with its own `CMakeLists.txt` + `idf_component_register`),
  then added to `main`'s `REQUIRES`.
- Managed dependencies (ESP Component Registry) are declared in `main/idf_component.yml`.
  `esp_tinyusb` is active for USB HID; future Opus work should use
  `espressif/esp_audio_codec^2.5.0`, not vendored `xiph/opus`.

Module mapping (Pico → ESP-IDF):

| Pico module (`legacy-pico/src`) | ESP-IDF target |
|---|---|
| `bt.cpp`, `btstack_config.h` | Bluedroid Classic (BR/EDR) HID **host** (biggest rewrite: BTstack API → Bluedroid callbacks) |
| `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | `esp_tinyusb` device (descriptors port closely) |
| `audio.cpp` | default-disabled audio scaffold now; future UAC + Opus via `esp_audio_codec` after hardware validation |
| `config.cpp` | NVS |
| `cmd.cpp`, `state_mgr.cpp` | port mostly as-is |
| `battery_led.cpp` | plain GPIO path with disabled default pin; `led_strip` only after hardware requires it |
| `main.cpp` | `app_main` + FreeRTOS tasks |

All OpenSpec implementation groups for `port-firmware-esp32s31` are complete,
the main specs are synced, and the change is archived under
`openspec/changes/archive/2026-06-15-port-firmware-esp32s31/`. Suggested next
order: physical S31 hardware bring-up, then a new scoped change for any runtime
audio or board-specific LED work.

## Remaining hardware TODOs

- CI now uses the GHCR image `ghcr.io/nyavana/ds5dongle-esp32s31-idf:master` and
  emits ESP32-S31 `.bin` artifacts. Run the `Build ESP-IDF S31 container` workflow
  once before relying on normal firmware CI, because the image must exist in GHCR.
- Validate HID-only USB enumeration on physical ESP32-S31 hardware.
- Validate BR/EDR pairing and HID report traffic with a DualSense controller.
- Identify the real board LED GPIO and set `DS5_BATTERY_LED_GPIO` if hardware has
  a suitable indicator.
- Decide whether to enable HID+UAC descriptors and live Opus audio after hardware
  USB and Bluetooth paths are stable.

Because this tracks ESP-IDF `master`, expect APIs/Kconfig to shift; re-check after any
IDF update and switch to the first tagged release that lists `esp32s31` once one ships.
