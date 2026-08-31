# DS5Dongle-ESP — ESP32-S31

> Turn an **ESP32-S31** into a wireless DualSense (DS5) dongle.
> ESP-IDF firmware for the ESP32-S31 target; the Pico 2W implementation is
> preserved only as the reference source.

**Status:** build-verified ESP32-S31 firmware scaffold. Boot, USB HID, BR/EDR
HID host, report bridge, NVS config/commands, state management, audio scaffold,
battery LED scaffold, and CI wiring build clean for `esp32s31`. Runtime hardware
bring-up remains pending. See [`docs/MIGRATION.md`](docs/MIGRATION.md) and
[`docs/COMPLETION.md`](docs/COMPLETION.md).

## Why the ESP32-S31

The dongle needs **Bluetooth Classic (BR/EDR)** to talk to the DualSense *and*
**USB-OTG device** mode to present itself to the host. The S31 is the first
single-chip ESP target with both (BT 5.4 Classic + USB-OTG HS, 320 MHz native).
The S3 is BLE-only; the original ESP32 lacks native USB — see the migration doc.

## Layout

```
DS5Dongle-ESP/            (bare-repo worktree root — see ../LAYOUT.md)
  master/                 Pico 2W reference (upstream, pristine)
  esp32-s31/  <-- here    ESP-IDF ESP32-S31 firmware project
    CMakeLists.txt        ESP-IDF top-level
    sdkconfig.defaults    reproducible config defaults
    partitions.csv
    main/                 app_main entry (thin)
    components/           ported subsystems (usb / bthost / bridge / config / ...)
    tools/                portable config tool and Wireshark post-dissector
    tests/host/           dependency-free config/command contract tests
    legacy-pico/          original Pico sources, kept for side-by-side porting
    docs/MIGRATION.md     module-by-module port plan
    .devcontainer/        ESP-IDF Docker dev environment
```

## Build

Requires ESP-IDF with ESP32-S31 support (see "Toolchain" below).

```bash
# esp32s31 is a PREVIEW target in IDF master, so the --preview flag is required:
idf.py --preview set-target esp32s31
idf.py build
idf.py -p <PORT> flash monitor
```

Or open the folder in VS Code with the Dev Containers extension — it uses the
local `espidf:s31` image (built below).

### Toolchain

ESP32-S31 is new — as of 2026-06 the `esp32s31` target exists **only in ESP-IDF
`master`** (a *preview* target; no tagged release or stock `espressif/idf` Docker
tag has it yet). So you need a master-based IDF. Two options:

- **Docker (recommended)** — build a master-based image with only the S31
  toolchain (this is what the devcontainer uses):
  ```bash
  docker build -t espidf:s31 \
    --build-arg IDF_CLONE_BRANCH_OR_TAG=master \
    --build-arg IDF_INSTALL_TARGETS=esp32s31 \
    --build-arg IDF_CLONE_SHALLOW=1 \
    "https://github.com/espressif/esp-idf.git#master:tools/docker"
  # then, from this dir:
  docker run --rm -v "$PWD":/project -w /project espidf:s31 \
    bash -lc 'idf.py --preview set-target esp32s31 && idf.py build'
  ```
- **Native install** — clone ESP-IDF `master` and run `./install.sh esp32s31`,
  then `. ./export.sh` (needs a supported Python, ~3.9–3.13).

Verified 2026-06-15 on `espidf:s31` (IDF v6.2.0 / master): the current
ESP32-S31 firmware scaffold builds clean for `esp32s31` and produces
`ds5dongle_esp32s31.bin`. Runtime pairing, USB enumeration against a real host,
audio, and LED behavior still require physical ESP32-S31 hardware.

### CI container image

GitHub Actions builds firmware inside
`ghcr.io/nyavana/ds5dongle-esp32s31-idf:master`. Publish or refresh that image by
running the `Build ESP-IDF S31 container` workflow. The firmware workflow cannot
pull the container until that image exists in GHCR.

## Configuration tool

The firmware uses the canonical packed version-5 configuration contract: a
22-byte HID body stored in a validated 32-byte NVS envelope. Incompatible or
corrupt records fall back to complete version-5 defaults; valid zero values are
preserved. The status-GPIO fields are stored and reported for compatibility but
do not configure or drive a GPIO in this firmware slice.

Install `hidapi` only for live device access, then use the portable tool from
this worktree:

```bash
python3 -m pip install hidapi
python3 tools/config_tool.py fields
python3 tools/config_tool.py get
python3 tools/config_tool.py set speaker_volume=90 inactive_time=0
```

The tool selects the Generic Desktop/Game Pad interface, preserves fields not
named by `set`, verifies version 5 on read-back, and sends descriptor-sized
64-byte feature reports. Its pack/unpack logic and the firmware's pure config
and command contracts can be tested without HID bindings or hardware:

```bash
python3 -m unittest discover -s tests/host -p 'test_*.py'
cmake -S tests/host -B /tmp/ds5-s31-host-tests
cmake --build /tmp/ds5-s31-host-tests
ctest --test-dir /tmp/ds5-s31-host-tests --output-on-failure
```

These checks do not validate physical NVS, USB reconnect, Bluetooth, wake,
audio, or GPIO behavior.

## Pico Reference

This worktree is for the ESP32-S31 firmware. The Pico 2W original — including
its full README, web config, release `.uf2`s and credits — lives on the `master`
branch / `../master` worktree, and the sources are mirrored under
[`legacy-pico/`](legacy-pico/) for comparison only.
Upstream: <https://github.com/nyavana/DS5Dongle-ESP>.

Credits carried from the original:
- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense) — inspiration
- [egormanga/SAxense](https://github.com/egormanga/SAxense) — Bluetooth haptics POC
- [DualSense report docs](https://controllers.fandom.com/wiki/Sony_DualSense)
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX) — speaker report packet

## License

See [LICENSE](LICENSE) (unchanged from upstream).
