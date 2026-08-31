# Group 6 Audio Decision

Status: decided for the current migration plan.

The old decision point was whether to vendor `xiph/opus`, vendor WDL, inject
TinyUSB UAC support, or skip audio entirely. That is no longer the active plan.
The current plan is a buildable audio scaffold:

- add `components/audio` behind the existing `audio.h` API;
- preserve `audio_init`, `set_headset`, FreeRTOS task/queue shape, `portMUX`
  guarded state, counters, and `0x36` packet construction helpers;
- keep USB UAC capture, live haptics resampling, and Opus speaker encode
  default-off until ESP32-S31 hardware is available;
- do not vendor `xiph/opus` in this pass;
- use `espressif/esp_audio_codec^2.5.0` for the future Opus encode path;
- keep the USB descriptor HID-only until HID+UAC composite enumeration can be
  validated on real S31 hardware.

Rationale:

- The controller path already builds clean without audio and should not be
  destabilized by an untestable HID+UAC composite change.
- `esp_tinyusb` builds TinyUSB audio class code, but the project still needs real
  S31 USB-device validation before changing the descriptor/control surface.
- Audio timing is the least reliable thing to claim without hardware because it
  depends on host UAC scheduling, Bluetooth report timing, and controller
  behaviour.

Current decision status:

1. **Exact board/module**: deferred to hardware bring-up. Use a plain GPIO battery
   LED path with the pin disabled by default, and keep flash/PSRAM tunable.
2. **USB bring-up target**: decided as HID-only first. HID+UAC is a later hardware
   bring-up step after HID-only enumeration works on the intended host.
3. **Bluetooth host path**: decided as `esp_hidh` behind `bt.h`. Raw Bluedroid
   L2CAP is only a later fallback if real DualSense report traffic fails.
4. **Audio runtime**: deferred to hardware bring-up. Keep the scaffold default-off,
   do not vendor WDL, and defer UAC, haptics resampling, and Opus timing work.
5. **CI packaging**: decided. Use a published GHCR toolchain image,
   `ghcr.io/nyavana/ds5dongle-esp32s31-idf:master`, built by
   `.github/workflows/build-toolchain-image.yml`. Run that workflow once before relying
   on normal firmware CI, because the image must exist before `build.yml` can pull it.
