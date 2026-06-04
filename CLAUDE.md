# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An **ESP-IDF port to the ESP32-S31** of the Pico 2W DualSense bridge firmware.

**Status: scaffold** — project skeleton + toolchain only. No subsystem is ported yet;
`main/main.cpp` just boots and logs. The working/shipping firmware is the Pico 2W
build on the upstream `master` branch.

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
stays reproducible across worktrees / CI / the devcontainer. Treat every symbol there
as a **starting point** to confirm in `idf.py menuconfig` (the S31 is new and some
Kconfig symbols carry chip-specific suffixes or may not exist yet). Partition layout
is in `partitions.csv`.

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
- Managed dependencies (ESP Component Registry) are declared in `main/idf_component.yml`
  (`esp_tinyusb` for USB; Opus package id is a TODO).

Module mapping (Pico → ESP-IDF):

| Pico module (`legacy-pico/src`) | ESP-IDF target |
|---|---|
| `bt.cpp`, `btstack_config.h` | Bluedroid Classic (BR/EDR) HID **host** (biggest rewrite: BTstack API → Bluedroid callbacks) |
| `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | `esp_tinyusb` device (descriptors port closely) |
| `audio.cpp` | Opus component + I2S (S31 HW BT-audio sync may remove the manual resample timing) |
| `config.cpp` | NVS |
| `cmd.cpp`, `state_mgr.cpp` | port mostly as-is |
| `battery_led.cpp` | `led_strip` (RMT) or GPIO |
| `main.cpp` | `app_main` + FreeRTOS tasks |

Suggested porting order (per the migration doc): boot/logging on real HW → `usb`
(enumerate as DualSense) → `bt` (connect to controller, bridge input) → `state_mgr`
+ `cmd` (output reports) → `audio` → `config` (NVS) + `battery_led`.

## Open migration TODOs

- CI under `.github/workflows/` still builds the Pico `.uf2` (identical to `master`);
  port it to `idf.py build`.
- Confirm `sdkconfig.defaults` Kconfig symbols (`CONFIG_BT_CLASSIC_ENABLED`, CPU-freq
  symbol, etc.) in `menuconfig`.
- Resolve the Opus registry package id/version in `main/idf_component.yml`.

Because this tracks ESP-IDF `master`, expect APIs/Kconfig to shift; re-check after any
IDF update and switch to the first tagged release that lists `esp32s31` once one ships.
