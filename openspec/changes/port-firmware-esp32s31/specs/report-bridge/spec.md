## ADDED Requirements

### Requirement: Input report relay (BT → USB)

The firmware SHALL relay the DualSense BT input report `0x31` to the USB host as HID in report `0x01` (63-byte payload taken from `data + 3`). It SHALL detect the headset-presence bit in the report and notify the audio subsystem when it changes, and SHALL notify the battery indicator on each fresh report.

#### Scenario: Input report forwarded to host

- **WHEN** a BT interrupt input report with second byte `0x31` arrives and USB HID is ready
- **THEN** the 63-byte controller state is copied into the in-report buffer and sent to the host as HID report `0x01`

#### Scenario: Headset state change propagated

- **WHEN** the headset-presence bit in the incoming report differs from the last value
- **THEN** the firmware updates the audio subsystem's headset state

### Requirement: Polling-rate modes

The firmware SHALL support the configured polling-rate modes: modes `0` (250 Hz) and `1` (500 Hz) forward each report directly, while mode `2` (real-time) uses a guarded dirty-flag buffer so the USB in-report path sends only freshly updated data, re-arming the dirty flag if the queue attempt fails.

#### Scenario: Direct forwarding mode

- **WHEN** `polling_rate_mode` is `0` or `1` and an input report arrives
- **THEN** the report is copied to the in-report buffer and forwarded without dirty-flag gating

#### Scenario: Real-time mode only sends fresh data

- **WHEN** `polling_rate_mode` is `2`
- **THEN** the in-report send path transmits only when the dirty flag is set (under a critical section), clearing it on success and restoring it if the USB queue attempt fails

### Requirement: Output report relay (USB → BT) and state management

The firmware SHALL build DualSense output report `0x31` from host output reports via the output-state manager (`state_mgr`), incrementing the report sequence counter, and send it over Bluetooth. It SHALL suppress standalone `0x31` output only when a future, validated audio subsystem explicitly reports that the speaker audio interface is active; the default audio scaffold keeps speaker inactive.

#### Scenario: Host output becomes a BT 0x31 report

- **WHEN** the host sends output report `0x02` and the speaker interface is not active
- **THEN** the firmware constructs a `0x31` output report with the next sequence number and current set-state data and writes it to the controller

#### Scenario: Output suppressed during active speaker stream

- **WHEN** the host sends output report `0x02` while a future validated audio path reports the speaker interface active
- **THEN** the standalone `0x31` output report is not sent (output state rides in the audio `0x36` report instead)
