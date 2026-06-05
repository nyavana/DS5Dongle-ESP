## Why

The shipping DualSense (DS5) adapter firmware runs on the Raspberry Pi Pico 2W, but the only single-chip ESP that can replace it — being simultaneously a **Bluetooth Classic (BR/EDR) HID host** to the controller and a **USB HID device** to the console — is the **ESP32-S31** (BT 5.4 Classic + USB-OTG; the S3 is BLE-only, the original ESP32 has no USB device). The `esp32-s31` branch currently has only a build scaffold: `app_main()` boots and logs, and no subsystem is ported. This change ports the full firmware so the S31 build is functionally equivalent to the Pico 2W build.

The work runs **autonomously (no human in the loop)**. The agent has the `espidf:s31` (ESP-IDF `master`) Docker toolchain for build verification but **no physical S31 hardware**, so "done" means a clean `idf.py build` for the `esp32s31` preview target plus a logically complete port — on-hardware bring-up of BT/USB/audio is deferred to a human and reported, not claimed.

## What Changes

- Replace the Pico super-loop (`main()` polling `cyw43_arch_poll` / `tud_task` / `audio_loop` / `interrupt_loop`) with an ESP-IDF `app_main()` + FreeRTOS tasks driven by Bluedroid and TinyUSB event callbacks.
- Port each Pico module into its own ESP-IDF component under `components/<name>/`, dropping all Pico SDK / BTstack / CYW43 dependencies.
- **BREAKING** — rewrite the Bluetooth HID host: the Pico code is a hand-rolled BR/EDR HID host over **raw BTstack L2CAP** (manual inquiry by class-of-device, PIN/link-key/user-confirmation pairing, explicit channels on PSM 0x11 / 0x13). It is re-implemented on **Bluedroid** Classic (BR/EDR) — the largest single piece of work.
- Emulate the DualSense as a **USB device** via the `esp_tinyusb` managed component: USB + HID + UAC audio descriptors, enumeration, and HID/audio control callbacks.
- Bridge HID reports both ways: BT input report `0x31` → USB HID in report `0x01` (with the 250 Hz / 500 Hz / real-time polling-rate modes), and USB output reports → BT output reports (rumble/LED via `state_mgr`).
- Persist configuration in **NVS** (replacing flash) and keep the custom `0xf6` feature-report command protocol; pair-key/bonding storage also moves to NVS.
- Port the audio path to **Opus (ESP Component Registry) + I2S**, and the low-battery indicator to board GPIO / `led_strip`.
- Replace Pico HW calls: `vreg`/`set_sys_clock_khz` → sdkconfig CPU freq; `hardware/watchdog` → `esp_task_wdt`; `critical_section_t` → `portMUX_TYPE`; `board_init`/`tud_*` → `esp_tinyusb`.
- Update build inputs: `main/idf_component.yml` (esp_tinyusb, Opus), `sdkconfig.defaults` (BT Classic, USB-OTG, CPU freq, NVS, task WDT), `partitions.csv` (NVS), and port `.github/workflows/` from the Pico `.uf2` build to `idf.py build`.

## Capabilities

### New Capabilities
- `firmware-boot`: S31 target/clock/sdkconfig selection, `app_main()` startup sequence, the FreeRTOS task architecture that replaces the super-loop, logging, and the task watchdog.
- `usb-device`: `esp_tinyusb` device emulating a DualSense — USB/HID/UAC descriptors, enumeration, and HID GET/SET-report + audio control callbacks.
- `bt-hid-host`: Bluedroid BR/EDR HID-host link to the DualSense — controller/stack bring-up, discovery, pairing/bonding, HID control + interrupt channels, RSSI.
- `report-bridge`: bidirectional HID report relay — BT input → USB in reports with polling-rate modes, USB output reports → BT output reports, sequence counting, and `state_mgr` output-state handling.
- `device-config`: NVS-persisted `Config` (defaults, version, CRC validation) plus the custom feature-report command interface (`cmd.cpp`) that reads/writes it.
- `audio-bridge`: Opus encode/decode + I2S speaker/mic path with headset enable and the S31 HW BT-audio sync.
- `battery-indicator`: low-battery LED indicator derived from the parsed DualSense input report.

### Modified Capabilities
<!-- None — openspec/specs/ is empty; every capability above is new. -->

## Impact

- **Code**: `main/main.cpp` rewritten from the boot stub into `app_main()` + tasks; new components `components/{bt,usb,audio,config,cmd,state_mgr,battery_led}/` (each with `CMakeLists.txt` + `idf_component_register`); `legacy-pico/src` stays as read-only reference.
- **Build / config**: `main/idf_component.yml`, `sdkconfig.defaults`, `partitions.csv`, `main/CMakeLists.txt` REQUIRES, `.github/workflows/`.
- **Dependencies added**: Bluedroid (BR/EDR HID host), `esp_tinyusb`, Opus component, `led_strip`. **Dropped**: Pico SDK, `cyw43_arch`, BTstack, `bsp/board_api`, WDL (resampler vendored only if still needed).
- **Toolchain**: ESP-IDF `master` only — `esp32s31` is a `--preview` target absent from any tagged release/stock image. Expect Kconfig/API drift; re-verify after IDF updates.
- **Verification limit**: autonomous build verification only (clean `esp32s31` build); BR/EDR pairing, USB enumeration, and audio require physical S31 hardware and are out of scope for this run — flagged in the final report for human bring-up.
