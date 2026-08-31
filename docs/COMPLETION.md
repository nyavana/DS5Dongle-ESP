# ESP32-S31 Migration Completion Report

Date: 2026-06-15

This report closes the initial autonomous ESP32-S31 migration pass.
Verification here means a clean ESP-IDF build for the `esp32s31` preview target
using the local `espidf:s31` toolchain. No physical ESP32-S31 hardware was
available, so runtime BT, USB, audio, and LED behavior are not claimed as
tested.

## Build-Verified Scope

- Boot path: thin `app_main()`, NVS init, report buffer init, ESP logging, and
  task-watchdog-backed FreeRTOS task model.
- USB device: HID-only DualSense descriptors through `esp_tinyusb`; UAC remains
  disabled by default.
- Bluetooth host: Bluedroid BR/EDR HID host through `esp_hidh`, behind `bt.h`,
  with discovery, pairing/bonding, input callback delivery, output writes, RSSI,
  and feature-report cache.
- Report bridge: BT `0x31` input to USB `0x01`, USB output `0x02` to BT `0x31`,
  polling-rate modes, state packing, command routing, and headset/battery hooks.
- Device config: NVS-backed config blob with validation, read-back save verify,
  and `0xf6`-`0xf9` feature-report commands.
- Audio scaffold: `components/audio` behind `audio.h`, default-disabled runtime,
  FreeRTOS queue/task shape, headset state, guarded speaker payload, counters,
  and `0x36` packet helper.
- Battery indicator: `components/battery_led` behind `battery_led.h`, plain GPIO
  path, `DS5_BATTERY_LED_GPIO=-1` disabled default, fresh-report timeout,
  disconnect clear, and `disable_pico_led` honoring.
- CI packaging: firmware workflows target the published GHCR S31 toolchain image
  path, with a separate image-build workflow available.

## Defaults Chosen

- Bluetooth: `esp_hidh` first; raw Bluedroid L2CAP remains a hardware fallback
  if real DualSense traffic fails.
- USB: HID-only descriptors for the autonomous build; HID+UAC composite
  validation is deferred.
- Audio: no vendored WDL, no vendored `xiph/opus`, no default-on UAC capture, no
  live Opus encode, and no live `0x36` transmit. Future Opus work should use
  `espressif/esp_audio_codec^2.5.0`.
- Battery LED: plain ESP-IDF GPIO, disabled by default until the board LED pin is
  known. Do not add `led_strip` unless the selected board requires it.

## Hardware Bring-Up Deferrals

- Confirm BR/EDR pairing, reconnect, bond persistence, raw input report delivery,
  output report writes, RSSI updates, and controller feature reports on real S31
  hardware.
- Confirm HID-only USB enumeration is accepted by the intended host as a
  DualSense-compatible controller.
- Enable TinyUSB UAC only after hardware is available; then validate HID+UAC
  composite enumeration and audio control requests.
- Feed 4-channel host audio into the scaffold, validate haptics resampling, enable
  `esp_audio_codec` Opus encode, and test `0x36` Bluetooth audio timing.
- Set the actual `DS5_BATTERY_LED_GPIO` and test low-battery blink, staleness
  timeout, disconnect clear, and `disable_pico_led` behavior.
- Re-run config command, USB reconnect, and inactivity-disconnect workflows
  against hardware before claiming runtime success.
