# components/

Ported subsystems live here, one ESP-IDF component per Pico module or shared
runtime boundary. Keep `main/` thin and move logic into these components.

| Component | Status | Ported from `legacy-pico/src` | ESP-IDF surface |
|---|---|---|---|
| `usb` | ported | `usb.cpp`, `usb_descriptors.cpp`, `tusb_config.h` | `esp_tinyusb` HID device with custom DualSense descriptors |
| `bthost` | ported | `bt.cpp` | Bluedroid Classic (BR/EDR) HID host via `esp_hidh` |
| `bridge` | ported | `main.cpp` interrupt/report loop | BT input to USB HID input, USB output to BT output |
| `report_buffer` | ported | `main.cpp` globals | `portMUX`-guarded shared input-report buffer |
| `config` | ported | `config.cpp` | NVS blob with validation and read-back verification |
| `cmd` | ported | `cmd.cpp` | custom HID feature-report command protocol |
| `state_mgr` | ported | `state_mgr.cpp` | DualSense output-state packing |
| `common` | ported | utility helpers | small shared helpers without legacy transport dependencies |
| `audio` | scaffold ported | `audio.cpp` | default-disabled FreeRTOS queue/task, headset state, guarded speaker buffer, and `0x36` helper; UAC/Opus runtime deferred |
| `battery_led` | ported | `battery_led.cpp` | plain GPIO path with disabled default pin, fresh-report timeout, disconnect clear, respecting `disable_pico_led` |

Each component has its own `CMakeLists.txt` with `idf_component_register(...)`.
See `docs/MIGRATION.md` and `docs/COMPLETION.md` for remaining work.
