## ADDED Requirements

### Requirement: DualSense USB device emulation

The firmware SHALL enumerate to a USB host as a DualSense controller using the `esp_tinyusb` managed component, presenting the same device/configuration/HID/UAC descriptors as the Pico build (ported from `usb_descriptors.cpp` / `tusb_config.h`). USB device bring-up SHALL use `esp_tinyusb` init in place of `board_init` / `tusb_init`.

#### Scenario: Host enumerates the dongle as a DualSense

- **WHEN** the dongle is attached to a USB host
- **THEN** it enumerates with the DualSense VID/PID and descriptor set, exposing the HID interface and the UAC audio interfaces

#### Scenario: TinyUSB is driven by esp_tinyusb, not the Pico BSP

- **WHEN** the USB component source is inspected
- **THEN** initialization uses `esp_tinyusb` / TinyUSB device callbacks and no `bsp/board_api.h` / `board_init` calls remain

### Requirement: HID GET_REPORT handling

The firmware SHALL service HID GET_REPORT (feature) requests: custom command report IDs (`0xf6`–`0xf9`) route to the device-config command interface, and other feature report IDs return their stored feature data.

#### Scenario: Custom command feature read

- **WHEN** the host issues GET_REPORT for a command report ID (e.g. `0xf7` config, `0xf8` firmware version, `0xf9` RSSI)
- **THEN** the firmware returns the corresponding command payload and length

#### Scenario: Standard feature read

- **WHEN** the host issues GET_REPORT for a non-command feature report ID with stored data
- **THEN** the firmware returns the stored feature payload, or STALLs (length 0) when none exists

### Requirement: HID SET_REPORT and OUT handling

The firmware SHALL service HID SET_REPORT / interrupt-OUT traffic: command report IDs route to the device-config command interface; output report `0x02` updates output state and triggers a BT output report; feature report IDs `0x80`/`0x60`/`0x61`/`0x62` store feature data.

#### Scenario: Output report triggers BT output

- **WHEN** the host sends an interrupt-OUT report with leading byte `0x02`
- **THEN** the firmware updates the output state and, unless the speaker interface is active, sends a `0x31` output report to the controller over Bluetooth

#### Scenario: Command write applies config

- **WHEN** the host sends a SET_REPORT for command report `0xf6`
- **THEN** the firmware dispatches it to the device-config command handler

### Requirement: USB audio control interface

The firmware SHALL expose the UAC audio control entities (speaker and mic mute/volume) and track the active speaker alternate setting so the audio path knows when the host has opened the speaker stream.

#### Scenario: Speaker interface activation tracked

- **WHEN** the host sets the speaker streaming interface to a non-zero alternate setting
- **THEN** the firmware marks the speaker active so audio frames are forwarded to the controller

#### Scenario: Mute/volume requests serviced

- **WHEN** the host issues a UAC get/set request for the speaker or mic mute/volume entity
- **THEN** the firmware updates or returns the corresponding mute/volume state
