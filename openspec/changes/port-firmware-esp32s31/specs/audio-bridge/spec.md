## ADDED Requirements

### Requirement: Audio component scaffold and deferred runtime boundary

The firmware SHALL provide a buildable `components/audio` implementation behind the existing `audio.h` API (`audio_init`, `set_headset`) and SHALL keep the runtime USB-UAC capture and Opus speaker encode path default-disabled until S31 hardware can validate HID+UAC composite enumeration and audio timing.

#### Scenario: Audio scaffold initializes safely

- **WHEN** firmware boots without UAC enabled
- **THEN** `audio_init` completes without changing USB descriptors, consuming host audio, sending `0x36` reports, or suppressing normal `0x31` output reports

#### Scenario: Headset state is retained

- **WHEN** the report bridge detects a headset-presence change
- **THEN** `set_headset` records the state for the future speaker/headset audio sub-mode

### Requirement: Audio bridge structure and packetization helpers

The firmware SHALL preserve the Pico audio bridge structure in a safe scaffold: FreeRTOS task/queue shape in place of `multicore_launch_core1`/`queue_t`, a `portMUX`-guarded latest speaker buffer in place of `opus_cs`, sequence/packet counters, and helpers for building DualSense audio report `0x36` with output-state, haptics, and speaker/headset sections.

#### Scenario: Packet helper preserves report shape

- **WHEN** future UAC/audio data supplies a full haptics block and latest speaker payload
- **THEN** the helper can build a `0x36` report with sequence/packet counters, current output state, haptics block, and speaker/headset sub-mode matching the stored headset state

### Requirement: Future UAC and Opus dependency plan

The firmware SHALL document the deferred runtime path: enable TinyUSB UAC descriptors/callbacks on hardware, read 4-channel host audio, apply speaker mute/volume and haptics gain, resample haptics from 48 kHz to 3 kHz, and use `espressif/esp_audio_codec^2.5.0` for future Opus speaker encode. The migration SHALL NOT vendor `xiph/opus` in this pass.
The migration SHALL NOT vendor WDL in this pass; haptics resampling remains an explicit hardware-follow-up stub.

#### Scenario: Runtime audio remains a hardware follow-up

- **WHEN** the completion report is written
- **THEN** it lists UAC enablement, composite enumeration validation, 4-channel audio capture, haptics resampling, Opus encode, and `0x36` BT timing as unverified hardware follow-up items
