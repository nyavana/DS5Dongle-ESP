# DS5Dongle - Pico 2W to ESP32-S31 migration

Status: **build-verified ESP32-S31 migration scaffold**.

The ESP-IDF project now builds clean for the `esp32s31` preview target with these
subsystems ported and wired:

- boot/app_main, NVS init, task watchdog, and report-buffer setup;
- USB HID device descriptors/callbacks through `esp_tinyusb`;
- BR/EDR HID host through Bluedroid `esp_hidh`;
- BT input to USB HID input bridge;
- USB output/feature reports to BT output reports;
- canonical version-5 NVS-backed config, bounded custom command protocol,
  portable host tooling, and output-state packing;
- default-disabled audio scaffold behind `audio.h`, including headset state,
  queue/task shape, guarded speaker payload state, counters, and `0x36` packet
  helper;
- plain GPIO low-battery LED scaffold behind `battery_led.h`, disabled by
  default until the board GPIO is known.

Still pending: runtime validation on physical ESP32-S31 hardware.

## Why ESP32-S31

The DualSense wireless link is **Bluetooth Classic (BR/EDR) HID**, and the dongle
must also be a **USB device** to the host. The Pico 2W did both using CYW43439
plus TinyUSB. Of the ESP32 family:

- **ESP32-S3** has native USB-OTG but is BLE-only, so it cannot talk to the
  DualSense BR/EDR HID profile.
- **Original ESP32** has BR/EDR but no native USB device peripheral.
- **ESP32-S31** has both Bluetooth Classic and native USB-OTG, plus a native
  320 MHz clock.

So the S31 is the first single-chip ESP target that fits this project.

## Source of truth for the original code

- `../master` worktree: pristine Pico 2W build and upstream shipping reference.
- `./legacy-pico/`: same sources mirrored on this branch for side-by-side porting.
  Submodules are not used by the current S31 build.

## Module mapping

| Pico module | Role | ESP32-S31 / ESP-IDF status |
|---|---|---|
| `main.cpp` | single super-loop and hardware setup | ported to thin `app_main()` plus FreeRTOS tasks/callbacks |
| `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | TinyUSB device posing as DualSense | ported as `components/usb` using `esp_tinyusb` and HID-only custom descriptors |
| `bt.cpp`, `btstack_config.h` | BTstack BR/EDR HID host | ported as `components/bthost` using Bluedroid `esp_hidh` |
| input/output bridge from `main.cpp` | BT `0x31` input to USB `0x01`, USB output to BT `0x31` | ported as `components/bridge` plus `components/report_buffer` |
| `config.cpp` | config persisted to raw flash | ported as a packed version-5 contract in `components/config` using NVS only |
| `cmd.cpp` | custom HID feature-report commands | ported as bounded `0xf6`-`0xf9` helpers in `components/cmd`; USB reconnect stays inside `components/usb` |
| `state_mgr.cpp` | DualSense output-state packing | ported as `components/state_mgr` |
| `audio.cpp` | UAC input, haptics resample, Opus speaker packets | scaffold ported as `components/audio`; runtime UAC/Opus deferred to hardware bring-up |
| `battery_led.cpp` | low-battery LED blink | ported as `components/battery_led`; GPIO disabled by default until board pin is known |

## Version-5 config and host contract

The S31 port now uses the canonical packed 22-byte `Config_body` and a packed
32-byte NVS envelope containing magic, CRC, body size, and body. Load accepts
only exact-size version-5 records with valid magic, size, and CRC. A matching
record is normalized field by field; incompatible version-1 or corrupt data is
reset to complete version-5 defaults rather than reinterpreted. Save writes and
commits the envelope, reads it back at the exact size, validates it again, and
compares the body before reporting success.

The two status-GPIO bytes are present for wire/storage compatibility. The pin
is normalized against the ESP32-S31 output-capable SoC predicate, but this task
does not select a board pin or configure/drive GPIO output.

`tools/config_tool.py` is the portable version-5 HID client. It loads `hidapi`
only for live access, filters for the gamepad interface, supports report bodies
with or without a report-ID prefix, preserves unspecified values, and writes
exactly 64 bytes per SET feature report. `tools/wireshark_dualsense_setstate.lua`
decodes the 47-byte SetState payload without depending on the Pico build.

The cross-language contract is covered without hardware by Python `unittest`
plus CMake/CTest targets under `tests/host/`. Physical NVS, USB reconnect,
Bluetooth, wake, audio, and GPIO behavior remains part of hardware bring-up.

## `main.cpp` hardware calls to ESP-IDF

| Pico call | Replacement |
|---|---|
| `vreg_set_voltage`, `set_sys_clock_khz(320000)` | `sdkconfig.defaults` CPU frequency; S31 runs 320 MHz natively |
| `cyw43_arch_*` BT bring-up | native S31 Bluetooth controller + Bluedroid |
| `cyw43_arch_gpio_put` LED | plain ESP-IDF GPIO in `components/battery_led`; pin disabled until real board is known |
| `hardware/watchdog.h` | `esp_task_wdt` |
| `critical_section_t` | `portMUX_TYPE` / `taskENTER_CRITICAL` |
| `board_init`, `tud_*` | `esp_tinyusb` init plus TinyUSB callbacks |
| super-loop polling | BT/USB event callbacks plus FreeRTOS tasks |

## Toolchain status

ESP32-S31 support is only in ESP-IDF `master` as a preview target:

- the target is `esp32s31`;
- it requires `idf.py --preview set-target esp32s31`;
- no tagged ESP-IDF release or stock `espressif/idf` image is assumed to contain
  it yet for this project.

Toolchain used here: a master-based Docker image `espidf:s31` (IDF v6.2.0),
built with:

```bash
docker build -t espidf:s31 \
  --build-arg IDF_CLONE_BRANCH_OR_TAG=master \
  --build-arg IDF_INSTALL_TARGETS=esp32s31 \
  --build-arg IDF_CLONE_SHALLOW=1 \
  "https://github.com/espressif/esp-idf.git#master:tools/docker"
```

Canonical local build gate:

```bash
docker run --rm -v "$PWD":/p -w /p espidf:s31 \
  idf.py --preview set-target esp32s31 build
```

Verified 2026-06-15: the current migration scaffold builds clean and produces
`build/ds5dongle_esp32s31.bin`. The generated app was `0x13eac0` bytes with about
59% free in the 3 MB app partition.

Because this tracks ESP-IDF `master`, re-check `sdkconfig.defaults` and component
APIs after any IDF image refresh. Switch to the first tagged ESP-IDF release that
officially lists `esp32s31` once that is available.

## Current open items

- **Future audio runtime**: once hardware is available, validate HID+UAC composite
  enumeration, read 4-channel UAC host audio, test haptics resampling, and enable
  Opus encode through `espressif/esp_audio_codec^2.5.0`. The current scaffold
  does not enable UAC, does not inject `CFG_TUD_AUDIO`, does not vendor
  `xiph/opus`, does not vendor WDL, and does not send live `0x36` audio by
  default.
- **Battery LED hardware pin**: the component is ported, but
  `DS5_BATTERY_LED_GPIO` defaults to `-1`. Set the real GPIO or switch to
  `led_strip` later only if the chosen board requires it.
- **Status GPIO behavior**: version 5 stores and reports the canonical
  `status_gpio_pin` and `status_gpio_mode` fields, but output remains inert until
  an actual board and board-safe allowlist are selected.
- **CI**: the workflows now build ESP32-S31 `.bin` artifacts using the published
  GHCR toolchain image `ghcr.io/nyavana/ds5dongle-esp32s31-idf:master`. Run
  `Build ESP-IDF S31 container` once before relying on normal firmware CI; until
  that image exists, firmware CI cannot pull its job container.
- **Runtime validation**: BR/EDR pairing, raw report pass-through, USB enumeration,
  config commands, reconnect behaviour, and audio timing all need real S31
  hardware before they can be called verified.

See [`COMPLETION.md`](COMPLETION.md) for the build-verified scope and the
hardware bring-up deferral list.

## Suggested Hardware Bring-Up Order

1. Run `Build ESP-IDF S31 container` once to publish or refresh the GHCR
   toolchain image if firmware CI cannot pull it.
2. When hardware arrives, run bring-up in this order: boot logs, USB HID
   enumeration, BR/EDR pairing, input/output report traffic, config commands,
   reconnect/bond persistence, then HID+UAC/audio.
