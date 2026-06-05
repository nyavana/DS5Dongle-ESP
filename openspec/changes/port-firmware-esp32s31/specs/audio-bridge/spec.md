## ADDED Requirements

### Requirement: Host audio capture and channel split

The firmware SHALL read 4-channel UAC audio delivered by the USB host, applying the configured speaker volume/mute gain to the speaker channels and the configured haptics gain to the haptics channels, replacing the Pico `tud_audio_read` path with the ESP TinyUSB audio API.

#### Scenario: Four-channel host audio split

- **WHEN** the host streams 4-channel audio and frames are available
- **THEN** the firmware extracts the speaker pair and the haptics pair, applying speaker gain (zeroed when muted) and haptics gain respectively

### Requirement: Haptics resampling and packetization

The firmware SHALL resample the haptics channels from 48 kHz to 3 kHz, convert to signed 8-bit, accumulate a 64-byte block, and emit it inside the DualSense audio output report `0x36` together with the current output state, sending it to the controller over Bluetooth.

#### Scenario: Haptics block sent to controller

- **WHEN** a full 64-byte haptics block has been accumulated
- **THEN** the firmware builds a `0x36` report (sequence/packet counters, set-state data, haptics block, and speaker block when enabled) and writes it to the controller over Bluetooth

### Requirement: Speaker Opus encoding on a second core

The firmware SHALL encode the speaker audio with Opus on a second core / dedicated task (mirroring the Pico `core1` encoder: 48 kHz stereo, 10 ms frames, ~200-byte frames), exchanging the encoded buffer with the report path under a critical section, and SHALL select the speaker vs. headset audio sub-mode based on the headset-presence state.

#### Scenario: Opus encode runs concurrently

- **WHEN** speaker audio frames are buffered
- **THEN** a separate core/task resamples and Opus-encodes them and publishes the latest encoded frame for the next `0x36` report under a guarded critical section

#### Scenario: Headset vs speaker routing

- **WHEN** the headset-presence state indicates a headset is connected
- **THEN** the audio block in the `0x36` report uses the headset sub-mode rather than the speaker sub-mode
