# DS5Dongle — Pico 2W → ESP32-S31 migration

Status: **scaffolding** (project skeleton + toolchain layout). No subsystem is
ported yet.

## Why ESP32-S31

The DualSense's wireless link is **Bluetooth Classic (BR/EDR) HID**, and the
dongle must also be a **USB device** to the host. The Pico 2W (CYW43439 + RP2350)
did both. Of the ESP32 family:

- **ESP32-S3** — has native USB-OTG but is **BLE-only** (no BR/EDR) → cannot talk
  to the DualSense. ✗
- **Original ESP32** — has BR/EDR but **no native USB device** peripheral. ✗
- **ESP32-S31** — has **both** Bluetooth 5.4 Classic (BR/EDR) **and** USB-OTG
  (high speed), plus 320 MHz native clock and HW BT-audio sync. ✓

So the S31 is the first single-chip ESP target that fits this project.

## Source of truth for the original code

- `../master` worktree — pristine Pico 2W build (also the upstream `master`).
- `./legacy-pico/` — same sources copied onto this branch for side-by-side
  porting. Submodules (`lib/opus`, `lib/WDL`) were dropped here; Opus comes from
  the ESP Component Registry, WDL's resampler can be vendored if still needed.

## Module mapping

| Pico module | Role | ESP32-S31 / ESP-IDF target | Notes |
|---|---|---|---|
| `bt.cpp`, `btstack_config.h` | BTstack BR/EDR **HID host** → DualSense | Bluedroid Classic HID host (`esp_hidh` / GAP BR/EDR) | Biggest rewrite: BTstack API → Bluedroid event callbacks |
| `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | TinyUSB **device** posing as DualSense | `esp_tinyusb` managed component | Descriptors port closely; TinyUSB underneath both |
| `audio.cpp` | Opus encode/decode + resample | `opus` component + I2S | S31 dual-I2S w/ HW BT-audio sync removes the manual resample timing pain |
| `config.cpp` | config persisted to flash | NVS | |
| `cmd.cpp` | custom HID feature-report commands | port as-is | |
| `state_mgr.cpp` | DualSense output-state mgmt | port as-is | |
| `battery_led.cpp` | low-battery LED blink | `led_strip` (RMT) or GPIO | CYW43 onboard LED → board LED |
| `main.cpp` | single super-loop + HW setup | `app_main` + FreeRTOS tasks | see below |

## `main.cpp` hardware calls → ESP-IDF

| Pico call | Replacement |
|---|---|
| `vreg_set_voltage`, `set_sys_clock_khz(320000)` | sdkconfig CPU freq (S31 native 320 MHz; no overclock) |
| `cyw43_arch_*` (BT bring-up + LED) | native S31 Bluetooth controller + board GPIO/LED |
| `hardware/watchdog.h` | `esp_task_wdt` |
| `critical_section_t` | `portMUX_TYPE` / `taskENTER_CRITICAL` |
| `board_init`, `tud_*` | `esp_tinyusb` init + TinyUSB device callbacks |
| super-loop polling | BT/USB event callbacks + dedicated FreeRTOS tasks |

## Toolchain status (verified 2026-06-03)

ESP32-S31 support is **only in ESP-IDF `master`**, as a **preview target**:

- `master` has the SoC target (`components/soc/esp32s31/...`) and a dedicated
  BT controller lib (`components/bt/controller/lib_esp32s31`). ✅
- It is in `PREVIEW_TARGETS` (`['linux', 'esp32h21', 'esp32h4', 'esp32s31']`),
  so it is **not** in `idf.py --list-targets` and requires the `--preview`
  flag: `idf.py --preview set-target esp32s31`.
- **No tagged release** includes it yet (latest are v6.0.1 / v5.5.4), and the
  stock `espressif/idf:latest` image (v6.1-dev) does not list it.

**Toolchain used here:** a master-based Docker image `espidf:s31` (IDF v6.2.0),
built with:

```bash
docker build -t espidf:s31 \
  --build-arg IDF_CLONE_BRANCH_OR_TAG=master \
  --build-arg IDF_INSTALL_TARGETS=esp32s31 \
  --build-arg IDF_CLONE_SHALLOW=1 \
  "https://github.com/espressif/esp-idf.git#master:tools/docker"
```

**Verified build (2026-06-03):** stock `hello_world` and this scaffold both
build clean for `esp32s31` on `espidf:s31`; `esp_tinyusb` compiles for the
target; output `ds5dongle_esp32s31.bin` (~179 KB).

Because this tracks `master`, expect APIs/Kconfig to shift and treat
BT-Classic/USB-on-S31 as not-yet-stable. Re-check after any IDF update, and
switch to the first tagged release that lists `esp32s31` once one ships.

## Open items / caveats

- **Target string**: scaffold assumes `idf.py set-target esp32s31` (present on
  IDF master). Confirm with `idf.py --list-targets`.
- **Kconfig symbols** in `sdkconfig.defaults` (`CONFIG_BT_CLASSIC_ENABLED`,
  CPU-freq symbol, etc.) are starting points — verify in `menuconfig`.
- **Opus registry package** id/version is a TODO in `main/idf_component.yml`.
- **CI**: `.github/workflows/*` still build the Pico `.uf2`; port to `idf.py build`.
- **Audio sync**: revisit the Pico's manual resampling now that the S31 offers
  hardware BT-audio synchronization.

## Suggested porting order

1. Boot + logging on real S31 hardware (validate target/clock/sdkconfig).
2. `usb` — enumerate as the DualSense to a host (no controller yet).
3. `bt` — BR/EDR HID host connect to the DualSense; bridge input reports to USB.
4. `state_mgr` + `cmd` — output reports (rumble/LED) back to the controller.
5. `audio` — Opus + I2S speaker/mic path.
6. `config` (NVS) + `battery_led` polish.
