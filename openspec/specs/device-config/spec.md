## Purpose

Defines the persistent configuration store and custom HID feature-report command
interface for the ESP32-S31 firmware.

## Requirements

### Requirement: Persisted configuration store

The firmware SHALL persist the `Config` structure (haptics gain, speaker volume, inactive time, auto-disconnect, LED disable, polling-rate mode, audio buffer length, controller mode) in **NVS** instead of raw flash sectors, replacing the Pico `hardware/flash` read/write path. Saves SHALL be verified after write.

#### Scenario: Config loaded at boot

- **WHEN** the firmware boots
- **THEN** it loads the stored configuration from NVS into memory, or falls back to defaults when no valid record exists

#### Scenario: Config saved and verified

- **WHEN** a config save is requested
- **THEN** the firmware writes the config to NVS and confirms the persisted record reads back correctly, reporting success or failure

### Requirement: Configuration validation and defaults

On load and on update the firmware SHALL validate every field against its allowed range and substitute the documented default for any invalid value (e.g. haptics gain to `1.0`, speaker volume to `-100`, inactive time to `30`, polling-rate mode to `0`, audio buffer length to `64`, controller mode to `2`), and SHALL maintain the magic/version/size header.

#### Scenario: Out-of-range field reset to default

- **WHEN** a loaded or updated field is outside its valid range (e.g. `polling_rate_mode > 2`)
- **THEN** the firmware replaces it with the field's default value before use

#### Scenario: Header normalized

- **WHEN** the stored magic, version, or body size does not match the expected constants
- **THEN** the firmware corrects the header fields so the record is well-formed

### Requirement: Custom feature-report command interface

The firmware SHALL implement the custom command protocol over HID feature reports `0xf6`-`0xf9`: `0xf7` returns the current config body, `0xf8` returns the firmware version string, `0xf9` returns link RSSI; a `0xf6` write sub-command `0x01` updates config in memory, `0x02` persists it, and `0x03` re-enumerates the USB device.

#### Scenario: Read current config

- **WHEN** the host issues GET_REPORT `0xf7`
- **THEN** the firmware returns the current `Config_body` (truncated to the requested length)

#### Scenario: Update then persist config

- **WHEN** the host sends SET_REPORT `0xf6` with sub-command `0x01` (payload) followed by `0x02`
- **THEN** the firmware applies the new config in memory (with validation) and then persists it to NVS

#### Scenario: USB re-enumeration command

- **WHEN** the host sends SET_REPORT `0xf6` with sub-command `0x03`
- **THEN** the firmware disconnects and reconnects the USB device so the host re-enumerates it
