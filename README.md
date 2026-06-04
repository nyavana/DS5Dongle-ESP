# DS5Dongle-ESP — ESP32-S31

> Turn an **ESP32-S31** into a wireless DualSense (DS5) dongle.
> Migration of the [Pico 2W DualSense bridge](#original-project) to ESP-IDF.

**Status:** 🚧 scaffolding — project skeleton + toolchain only, no subsystem
ported yet. See [`docs/MIGRATION.md`](docs/MIGRATION.md).

## Why the ESP32-S31

The dongle needs **Bluetooth Classic (BR/EDR)** to talk to the DualSense *and*
**USB-OTG device** mode to present itself to the host. The S31 is the first
single-chip ESP target with both (BT 5.4 Classic + USB-OTG HS, 320 MHz native).
The S3 is BLE-only; the original ESP32 lacks native USB — see the migration doc.

## Layout

```
DS5Dongle-ESP/            (bare-repo worktree root — see ../LAYOUT.md)
  master/                 Pico 2W reference (upstream, pristine)
  esp32-s31/  <-- here    ESP-IDF migration project
    CMakeLists.txt        ESP-IDF top-level
    sdkconfig.defaults    reproducible config defaults
    partitions.csv
    main/                 app_main entry (thin)
    components/           ported subsystems land here (bt / usb / audio / ...)
    legacy-pico/          original Pico sources, kept for side-by-side porting
    docs/MIGRATION.md     module-by-module port plan
    .devcontainer/        ESP-IDF Docker dev environment
```

## Build

Requires ESP-IDF with ESP32-S31 support (see "Toolchain" below).

```bash
idf.py set-target esp32s31      # confirm the exact name via `idf.py --list-targets`
idf.py build
idf.py -p <PORT> flash monitor
```

Or open the folder in VS Code with the Dev Containers extension to use the
pinned [`espressif/idf`](.devcontainer/devcontainer.json) image — no local
toolchain install needed.

### Toolchain

ESP32-S31 is new — as of 2026-06 the `esp32s31` target exists **only in ESP-IDF
`master`** (no tagged release or `espressif/idf` Docker tag has it yet). So you
need a master-based IDF. Two options:

- **Docker / devcontainer** — `espressif/idf` image; reproducible, nothing to
  install on the host besides Docker.
- **Native install** — clone ESP-IDF and run its installer, then `. export.sh`.

See [`docs/MIGRATION.md`](docs/MIGRATION.md) for caveats (target string, Kconfig
symbols, Opus component) that depend on the IDF version you land on.

## Original project

This is a port. The Pico 2W original — including its full README, web config,
release `.uf2`s and credits — lives on the `master` branch / `../master`
worktree, and the sources are mirrored under [`legacy-pico/`](legacy-pico/).
Upstream: <https://github.com/nyavana/DS5Dongle-ESP>.

Credits carried from the original:
- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense) — inspiration
- [egormanga/SAxense](https://github.com/egormanga/SAxense) — Bluetooth haptics POC
- [DualSense report docs](https://controllers.fandom.com/wiki/Sony_DualSense)
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX) — speaker report packet

## License

See [LICENSE](LICENSE) (unchanged from upstream).
