# components/

Ported subsystems live here, one ESP-IDF component per Pico module. Keep
`main/` thin and move logic into these as you port it.

| Component (planned) | Ported from `legacy-pico/src` | ESP-IDF surface |
|---|---|---|
| `bt`        | `bt.cpp`, `btstack_config.h`        | Bluedroid Classic (BR/EDR) HID host |
| `usb`       | `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | `esp_tinyusb` device |
| `audio`     | `audio.cpp`                          | Opus codec + I2S |
| `config`    | `config.cpp`                         | NVS |
| `state_mgr` | `state_mgr.cpp`                      | (port as-is) |
| `cmd`       | `cmd.cpp`                            | (port as-is) |
| `battery_led` | `battery_led.cpp`                  | `led_strip` / GPIO |

Each component needs its own `CMakeLists.txt` with `idf_component_register(...)`.
See docs/MIGRATION.md for the full mapping and per-module notes.
