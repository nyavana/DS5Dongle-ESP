## Purpose

Defines the ESP32-S31 Bluetooth Classic HID host path for discovering, pairing,
and exchanging DualSense reports.

## Requirements

### Requirement: BR/EDR controller and HID-host stack bring-up

The firmware SHALL bring up the ESP32-S31 native Bluetooth controller in **Classic (BR/EDR)** mode and initialize a Bluedroid-based HID **host** stack, replacing the Pico BTstack + `cyw43_arch` bring-up. The host SHALL act as the master to the DualSense over the HID Control (PSM `0x11`) and HID Interrupt (PSM `0x13`) L2CAP channels.

#### Scenario: Stack initializes in Classic mode

- **WHEN** `bt_init()` (ESP port) runs
- **THEN** the controller is enabled in BR/EDR mode, Bluedroid is initialized as a HID host, and no BTstack (`l2cap_*`, `gap_*`, `hci_send_cmd`) or `cyw43_arch_*` calls remain

#### Scenario: HID L2CAP channels established

- **WHEN** a DualSense connection is established
- **THEN** both the HID control and HID interrupt L2CAP channels (PSM `0x11` and `0x13`) are open for bidirectional report exchange

### Requirement: Controller discovery and pairing

The firmware SHALL discover and connect to a DualSense controller — filtering by Bluetooth class-of-device as the Pico inquiry did — and SHALL complete BR/EDR pairing (link-key / passkey / user-confirmation as required), persisting the bond so reconnection does not require re-pairing.

#### Scenario: DualSense discovered and connected

- **WHEN** a DualSense is in pairing mode and discovery runs
- **THEN** the firmware identifies it by class-of-device, connects, and completes pairing

#### Scenario: Bond persisted for reconnect

- **WHEN** a previously bonded DualSense powers on again
- **THEN** the firmware reconnects using the stored link key without a new pairing handshake

### Requirement: Input report delivery and link signal strength

The firmware SHALL deliver received HID interrupt input reports to a registered data callback (`channel`, `data`, `len`) so the report-bridge can forward them, and SHALL expose the current link RSSI.

#### Scenario: Input report reaches the bridge callback

- **WHEN** the controller sends a HID interrupt input report
- **THEN** the registered data callback is invoked with the interrupt channel, report bytes, and length

#### Scenario: RSSI query

- **WHEN** the firmware queries link signal strength
- **THEN** it returns the current RSSI for the active controller connection

### Requirement: Output report transmission

The firmware SHALL provide a `bt_write`-equivalent that transmits an output report to the controller over the HID interrupt channel, used for output state (`0x31`) and audio (`0x36`) reports.

#### Scenario: Output report sent to controller

- **WHEN** the report-bridge or audio path calls the BT write path with an output report buffer
- **THEN** the bytes are transmitted to the controller over the HID interrupt L2CAP channel
