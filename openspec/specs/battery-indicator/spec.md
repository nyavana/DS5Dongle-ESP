## Purpose

Defines the low-battery indicator behavior for ESP32-S31 boards.

## Requirements

### Requirement: Low-battery LED indication

The firmware SHALL drive a plain ESP-IDF GPIO LED path to indicate low controller battery, deriving battery percent/state from the parsed DualSense input report, replacing the Pico CYW43 onboard-LED path. The default build SHALL allow the LED GPIO to be disabled when no physical S31 board pin is known, and the indicator SHALL respect the `disable_pico_led` configuration option.

#### Scenario: Low battery blinks the LED

- **WHEN** the controller is connected and the latest input report indicates a low battery level
- **THEN** the firmware blinks the board LED to signal low battery

#### Scenario: LED disabled by config

- **WHEN** `disable_pico_led` is set
- **THEN** the firmware keeps the board LED off regardless of battery state

### Requirement: Disconnect and staleness handling

The firmware SHALL treat the cached battery byte as stale on disconnect — cancelling any in-progress blink, forcing the LED off, and ignoring the cached value until a fresh report arrives — and SHALL detect disconnection via a fresh-report timeout fed by the report-bridge.

#### Scenario: LED cleared on disconnect

- **WHEN** the controller disconnects
- **THEN** the firmware cancels any blink, forces the LED off, and ignores the stale battery byte until the next connection delivers a fresh report

#### Scenario: Freshness tracked from reports

- **WHEN** a fresh input report is received
- **THEN** the indicator records the report time so a subsequent timeout can detect a silent (disconnected) controller
